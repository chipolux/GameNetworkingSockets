//====== Copyright Valve Corporation, All rights reserved. ====================

#include "gamenetworkingsockets_p2p.h"
#include "cgamenetworkingsockets.h"
#include "../gamenetworkingsockets_certstore.h"
#include "crypto.h"

#ifdef _WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#undef min
	#undef max
#endif
#ifdef POSIX
	#include <dlfcn.h>
#endif

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
	#include "gamenetworkingsockets_sdr_p2p.h"
	#include "gamenetworkingsockets_sdr_client.h"
	#ifdef SDR_ENABLE_HOSTED_SERVER
		#include "gamenetworkingsockets_sdr_hostedserver.h"
	#endif
#endif

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
	#include "gamenetworkingsockets_p2p_ice.h"
	#ifdef STEAMWEBRTC_USE_STATIC_LIBS
		extern "C" IICESession *CreateWebRTCICESession( const ICESessionConfig &cfg, IICESessionDelegate *pDelegate, int nInterfaceVersion );
	#endif
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Put everything in a namespace, so we don't violate the one definition rule
namespace GameNetworkingSocketsLib {

// This table is protected by the global lock
CUtlHashMap<RemoteConnectionKey_t,CGameNetworkConnectionP2P*, std::equal_to<RemoteConnectionKey_t>, RemoteConnectionKey_t::Hash > g_mapP2PConnectionsByRemoteInfo;

constexpr GameNetworkingMicroseconds k_usecWaitForControllingAgentBeforeSelectingNonNominatedTransport = 1*k_nMillion;

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkListenSocketP2P
//
/////////////////////////////////////////////////////////////////////////////

CGameNetworkListenSocketP2P::CGameNetworkListenSocketP2P( CGameNetworkingSockets *pGameNetworkingSocketsInterface )
: CGameNetworkListenSocketBase( pGameNetworkingSocketsInterface )
{
}

CGameNetworkListenSocketP2P::~CGameNetworkListenSocketP2P()
{
	// Remove from virtual port map
	if ( m_connectionConfig.m_LocalVirtualPort.IsSet() )
	{
		int h = m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.Find( LocalVirtualPort() );
		if ( h != m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.InvalidIndex() && m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort[h] == this )
		{
			m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort[h] = nullptr; // just for grins
			m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.RemoveAt( h );
		}
		else
		{
			AssertMsg( false, "Bookkeeping bug!" );
		}
	}
}

bool CGameNetworkListenSocketP2P::BInit( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg )
{
	Assert( nLocalVirtualPort >= 0 );

	if ( m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.HasElement( nLocalVirtualPort ) )
	{
		V_sprintf_safe( errMsg, "Already have a listen socket on P2P vport %d", nLocalVirtualPort );
		return false;
	}
	m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.Insert( nLocalVirtualPort, this );

	// Lock in virtual port into connection config map.
	m_connectionConfig.m_LocalVirtualPort.Set( nLocalVirtualPort );
	m_connectionConfig.m_LocalVirtualPort.Lock();

	// Set options, add us to the global table
	if ( !BInitListenSocketCommon( nOptions, pOptions, errMsg ) )
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkConnectionP2P
//
/////////////////////////////////////////////////////////////////////////////

CGameNetworkConnectionP2P::CGameNetworkConnectionP2P( CGameNetworkingSockets *pGameNetworkingSocketsInterface, ConnectionScopeLock &scopeLock )
: CGameNetworkConnectionBase( pGameNetworkingSocketsInterface, scopeLock )
{
	m_nRemoteVirtualPort = -1;
	m_idxMapP2PConnectionsByRemoteInfo = -1;
	m_pSignaling = nullptr;
	m_usecWhenStartedFindingRoute = 0;
	m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
	m_bTransportSticky = false;

	m_pszNeedToSendSignalReason = nullptr;
	m_usecSendSignalDeadline = k_nThinkTime_Never;
	m_nLastSendRendesvousMessageID = 0;
	m_nLastRecvRendesvousMessageID = 0;
	m_pPeerSelectedTransport = nullptr;

	m_pCurrentTransportP2P = nullptr;
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		m_pTransportP2PSDR = nullptr;
		#ifdef SDR_ENABLE_HOSTED_CLIENT
			m_pTransportToSDRServer = nullptr;
		#endif
		#ifdef SDR_ENABLE_HOSTED_SERVER
			m_pTransportFromSDRClient = nullptr;
		#endif
	#endif
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		m_pTransportICE = nullptr;
		m_pTransportICEPendingDelete = nullptr;
		m_szICECloseMsg[ 0 ] = '\0';
	#endif
}

CGameNetworkConnectionP2P::~CGameNetworkConnectionP2P()
{
	Assert( m_idxMapP2PConnectionsByRemoteInfo == -1 );
}

void CGameNetworkConnectionP2P::GetConnectionTypeDescription( ConnectionTypeDescription_t &szDescription ) const
{
	if ( IsSDRHostedServerClient() )
		V_sprintf_safe( szDescription, "SDR server %s vport %d", GameNetworkingIdentityRender( m_identityRemote ).c_str(), m_nRemoteVirtualPort );
	else if ( m_pCurrentTransportP2P )
		V_sprintf_safe( szDescription, "P2P %s %s", m_pCurrentTransportP2P->m_pszP2PTransportDebugName, GameNetworkingIdentityRender( m_identityRemote ).c_str() );
	else
		V_sprintf_safe( szDescription, "P2P %s", GameNetworkingIdentityRender( m_identityRemote ).c_str() );
}

bool CGameNetworkConnectionP2P::BInitConnect(
	IGameNetworkingConnectionSignaling *pSignaling,
	const GameNetworkingIdentity *pIdentityRemote, int nRemoteVirtualPort,
	int nOptions, const GameNetworkingConfigValue_t *pOptions,
	CGameNetworkConnectionP2P **pOutMatchingSymmetricConnection,
	SteamDatagramErrMsg &errMsg
)
{
	Assert( !m_pTransport );

	if ( pOutMatchingSymmetricConnection )
		*pOutMatchingSymmetricConnection = nullptr;

	// Remember who we're talking to
	Assert( m_pSignaling == nullptr );
	m_pSignaling = pSignaling;
	if ( pIdentityRemote )
		m_identityRemote = *pIdentityRemote;
	m_nRemoteVirtualPort = nRemoteVirtualPort;

	// Remember when we started finding a session
	//m_usecTimeStartedFindingSession = usecNow;

	// Reset end-to-end state
	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();
	if ( !BInitP2PConnectionCommon( usecNow, nOptions, pOptions, errMsg ) )
		return false;

	// Check if there is a matching connection, for symmetric mode
	if ( !m_identityRemote.IsInvalid() && LocalVirtualPort() >= 0 )
	{
		bool bOnlySymmetricConnections = !BSymmetricMode();
		CGameNetworkConnectionP2P *pMatchingConnection = CGameNetworkConnectionP2P::FindDuplicateConnection( m_pGameNetworkingSocketsInterface, LocalVirtualPort(), m_identityRemote, m_nRemoteVirtualPort, bOnlySymmetricConnections, this );
		if ( pMatchingConnection )
		{
			if ( pOutMatchingSymmetricConnection )
				*pOutMatchingSymmetricConnection = pMatchingConnection;
			V_sprintf_safe( errMsg, "Existing symmetric connection [%s]", pMatchingConnection->GetDescription() );
			return false;
		}
	}
	else
	{
		if ( BSymmetricMode() )
		{
			Assert( LocalVirtualPort() >= 0 );
			V_strcpy_safe( errMsg, "To use symmetric connect, remote identity must be specified" );
			return false;
		}
	}

	if ( !BInitSDRTransport( errMsg ) )
		return false;

	// Check if we should try ICE.
	CheckInitICE();

	// No available transports?
	Assert( GetState() == k_EGameNetworkingConnectionState_None );
	if ( m_pTransport == nullptr && m_vecAvailableTransports.empty() )
	{
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
			EGameNetConnectionEnd ignoreReason;
			ConnectionEndDebugMsg closeDebugMsg;
			GuessICEFailureReason( ignoreReason, closeDebugMsg, usecNow );
			V_strcpy_safe( errMsg, closeDebugMsg );
		#else
			Assert( false ); // We shouldn't compile if we don't have either SDR or ICE transport enabled.  And if SDR fails, we fail above
			V_strcpy_safe( errMsg, "No available P2P transports" );
		#endif
		return false;
	}

	// Start the connection state machine
	return BConnectionState_Connecting( usecNow, errMsg );
}

bool CGameNetworkConnectionP2P::BInitP2PConnectionCommon( GameNetworkingMicroseconds usecNow, int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg )
{
	// Let base class do some common initialization
	if ( !CGameNetworkConnectionBase::BInitConnection( usecNow, nOptions, pOptions, errMsg ) )
		return false;

	// Check for defaulting the local virtual port to be the same as the remote virtual port
	if ( LocalVirtualPort() < 0 && m_nRemoteVirtualPort >= 0 )
		m_connectionConfig.m_LocalVirtualPort.Set( m_nRemoteVirtualPort );

	// Local virtual port cannot be changed henceforth
	m_connectionConfig.m_LocalVirtualPort.Lock();

	// Check for activating symmetric mode based on listen socket on the same local virtual port
	int nLocalVirtualPort = LocalVirtualPort();
	if ( nLocalVirtualPort >= 0 && !BSymmetricMode() )
	{

		// Are we listening on that virtual port?
		int idxListenSock = m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.Find( nLocalVirtualPort );
		if ( idxListenSock != m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort.InvalidIndex() )
		{

			// Really, they should match.  App code should be all-or-nothing.  It should not mix.
			if ( m_pGameNetworkingSocketsInterface->m_mapListenSocketsByVirtualPort[ idxListenSock ]->BSymmetricMode() )
			{
				SpewWarning( "[%s] Setting SymmetricConnect=1 because it is enabled on listen socket on vport %d.  To avoid this warning, specify the option on connection creation\n", GetDescription(), nLocalVirtualPort );
				Assert( !m_connectionConfig.m_SymmetricConnect.IsLocked() );
				m_connectionConfig.m_SymmetricConnect.Unlock();
				m_connectionConfig.m_SymmetricConnect.Set( 1 );
			}
		}
	}

	// Once symmetric mode is activated, it cannot be turned off!
	if ( BSymmetricMode() )
		m_connectionConfig.m_SymmetricConnect.Lock();

	// We must know our own identity to initiate or receive this kind of connection
	if ( m_identityLocal.IsInvalid() )
	{
		V_strcpy_safe( errMsg, "Unable to determine local identity.  Not logged in?" );
		return false;
	}

	// Check for connecting to self.
	if ( m_identityRemote == m_identityLocal )
	{
		// Spew a warning when P2P connecting to self
		// 1.) We should special case this and automatically create a pipe instead.
		//     But right now the pipe connection class assumes we want to be immediately
		//     connected.  We should fix that, for now I'll just spew.
		// 2.) It's not just connecting to self.  If we are connecting to an identity of
		//     another local CGameNetworkingSockets interface, we should use a pipe.
		//     But we'd probably have to make a special flag to force it to relay,
		//     for tests.
		SpewWarning( "Connecting P2P socket to self (%s).  Traffic will be relayed over the network", GameNetworkingIdentityRender( m_identityRemote ).c_str() );
	}

	// If we know the remote connection ID already, then  put us in the map
	if ( m_unConnectionIDRemote )
	{
		if ( !BEnsureInP2PConnectionMapByRemoteInfo( errMsg ) )
			return false;
	}

	return true;
}

void CGameNetworkConnectionP2P::RemoveP2PConnectionMapByRemoteInfo()
{
	AssertLocksHeldByCurrentThread();

	if ( m_idxMapP2PConnectionsByRemoteInfo >= 0 )
	{
		if ( g_mapP2PConnectionsByRemoteInfo.IsValidIndex( m_idxMapP2PConnectionsByRemoteInfo ) && g_mapP2PConnectionsByRemoteInfo[ m_idxMapP2PConnectionsByRemoteInfo ] == this )
		{
			g_mapP2PConnectionsByRemoteInfo[ m_idxMapP2PConnectionsByRemoteInfo ] = nullptr; // just for grins
			g_mapP2PConnectionsByRemoteInfo.RemoveAt( m_idxMapP2PConnectionsByRemoteInfo );
		}
		else
		{
			AssertMsg( false, "g_mapIncomingP2PConnections bookkeeping mismatch" );
		}
		m_idxMapP2PConnectionsByRemoteInfo = -1;
	}
}

bool CGameNetworkConnectionP2P::BEnsureInP2PConnectionMapByRemoteInfo( SteamDatagramErrMsg &errMsg )
{
	Assert( !m_identityRemote.IsInvalid() );
	Assert( m_unConnectionIDRemote );

	RemoteConnectionKey_t key{ m_identityRemote, m_unConnectionIDRemote };
	if ( m_idxMapP2PConnectionsByRemoteInfo >= 0 )
	{
		Assert( g_mapP2PConnectionsByRemoteInfo.Key( m_idxMapP2PConnectionsByRemoteInfo ) == key );
		Assert( g_mapP2PConnectionsByRemoteInfo[ m_idxMapP2PConnectionsByRemoteInfo ] == this );
	}
	else
	{
		if ( g_mapP2PConnectionsByRemoteInfo.HasElement( key ) )
		{
			// "should never happen"
			V_sprintf_safe( errMsg, "Duplicate P2P connection %s %u!", GameNetworkingIdentityRender( m_identityRemote ).c_str(), m_unConnectionIDRemote );
			AssertMsg1( false, "%s", errMsg );
			return false;
		}
		m_idxMapP2PConnectionsByRemoteInfo = g_mapP2PConnectionsByRemoteInfo.InsertOrReplace( key, this );
	}

	return true;
}

bool CGameNetworkConnectionP2P::BBeginAcceptFromSignal(
	const CMsgGameNetworkingP2PRendezvous_ConnectRequest &msgConnectRequest,
	SteamDatagramErrMsg &errMsg,
	GameNetworkingMicroseconds usecNow
) {
	m_bConnectionInitiatedRemotely = true;

	// Let base class do some common initialization
	if ( !BInitP2PConnectionCommon( usecNow, 0, nullptr, errMsg ) )
		return false;

	// Initialize SDR transport
	if ( !BInitSDRTransport( errMsg ) )
		return false;

	// Process crypto handshake now
	if ( !BRecvCryptoHandshake( msgConnectRequest.cert(), msgConnectRequest.crypt(), true ) )
	{
		Assert( GetState() == k_EGameNetworkingConnectionState_ProblemDetectedLocally );
		V_sprintf_safe( errMsg, "Error with crypto.  %s", m_szEndDebug );
		return false;
	}

	// Add to connection map
	if ( !BEnsureInP2PConnectionMapByRemoteInfo( errMsg ) )
		return false;

	// Start the connection state machine
	return BConnectionState_Connecting( usecNow, errMsg );
}

void CGameNetworkConnectionP2P::ChangeRoleToServerAndAccept( const CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow )
{
	int nLogLevel = LogLevel_P2PRendezvous();

	// Our connection should be the server.  We should change the role of this connection.
	// But we can only do that if we are still trying to connect!
	if ( GetState() != k_EGameNetworkingConnectionState_Connecting )
	{
		SpewWarningGroup( nLogLevel, "[%s] Symmetric role resolution for connect request remote cxn ID #%u says we should act as server.  But we cannot change our role, since we are already in state %d!  Dropping incoming request\n", GetDescription(), msg.from_connection_id(), GetState() );
		return;
	}

	// We should currently be the client, and we should not already know anything about the remote host
	if ( m_bConnectionInitiatedRemotely )
	{
		AssertMsg( false, "[%s] Symmetric role resolution for connect request remote cxn ID #%u says we should act as server.  But we are already the server!  Why haven't we transitioned out of connecting state.  Dropping incoming request\n", GetDescription(), msg.from_connection_id() );
		return;
	}

	SpewVerboseGroup( nLogLevel, "[%s] Symmetric role resolution for connect request remote cxn ID #%u says we should act as server.  Changing role\n", GetDescription(), msg.from_connection_id() );

	// !KLUDGE! If we already started ICE, then we have to nuke it and restart.
	// It'd be better if we could just ask ICE to change the role.
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		bool bRestartICE = false;
		CheckCleanupICE();

		// Already failed?
		if ( GetICEFailureCode() != 0 )
		{
			SpewVerboseGroup( nLogLevel, "[%s] ICE already failed (%d %s) while changing role to server.  We won't try again.",
				GetDescription(), GetICEFailureCode(), m_szICECloseMsg );
		}
		else if ( m_pTransportICE )
		{
			SpewVerboseGroup( nLogLevel, "[%s] Destroying ICE while changing role to server.\n", GetDescription() );
			DestroyICENow();
			bRestartICE = true;
			Assert( GetICEFailureCode() == 0 );
		}
	#endif

	// We should not have done the crypto handshake yet
	Assert( !m_unConnectionIDRemote );
	Assert( m_idxMapP2PConnectionsByRemoteInfo < 0 );
	Assert( !m_bCryptKeysValid );
	Assert( m_sCertRemote.empty() );
	Assert( m_sCryptRemote.empty() );

	// We're changing the remote connection ID.
	// If we're in the remote info -> connection map,
	// we need to get out.  We'll add ourselves back
	// correct when we accept the connection
	RemoveP2PConnectionMapByRemoteInfo();

	// Change role
	m_bConnectionInitiatedRemotely = true;
	m_unConnectionIDRemote = msg.from_connection_id();

	// Clear crypt stuff that we usually do immediately as the client, but have
	// to defer when we're the server.  We need to redo it, now that our role has
	// changed
	ClearLocalCrypto();

	// Process crypto handshake now -- acting as the "server"
	const CMsgGameNetworkingP2PRendezvous_ConnectRequest &msgConnectRequest = msg.connect_request();
	if ( !BRecvCryptoHandshake( msgConnectRequest.cert(), msgConnectRequest.crypt(), true ) )
	{
		Assert( GetState() == k_EGameNetworkingConnectionState_ProblemDetectedLocally );
		return;
	}

	// Add to connection map
	GameNetworkingErrMsg errMsg;
	if ( !BEnsureInP2PConnectionMapByRemoteInfo( errMsg ) )
	{
		ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_Misc_InternalError, "%s", errMsg );
		return;
	}

	// Restart ICE if necessary
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		if ( bRestartICE )
		{
			Assert( GetICEFailureCode() == 0 );
			CheckInitICE();
		}
	#endif

	// Accept the connection
	EResult eAcceptResult = APIAcceptConnection();
	if ( eAcceptResult == k_EResultOK )
	{
		Assert( GetState() == k_EGameNetworkingConnectionState_FindingRoute );
	}
	else
	{
		Assert( GetState() == k_EGameNetworkingConnectionState_ProblemDetectedLocally );
	}
}

CGameNetworkConnectionP2P *CGameNetworkConnectionP2P::AsGameNetworkConnectionP2P()
{
	return this;
}

bool CGameNetworkConnectionP2P::BInitSDRTransport( GameNetworkingErrMsg &errMsg )
{
	// Create SDR transport, if SDR is enabled
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

		// Make sure SDR client functionality is ready
		CGameNetworkingSocketsSDR *pGameNetworkingSocketsSDR = assert_cast< CGameNetworkingSocketsSDR *>( m_pGameNetworkingSocketsInterface );
		if ( !pGameNetworkingSocketsSDR->BSDRClientInit( errMsg ) )
			return false;

		// Create SDR transport
		Assert( !m_pTransportP2PSDR );
		m_pTransportP2PSDR = new CConnectionTransportP2PSDR( *this );
		Assert( !has_element( g_vecSDRClients, m_pTransportP2PSDR ) );
		g_vecSDRClients.push_back( m_pTransportP2PSDR );
		m_vecAvailableTransports.push_back( m_pTransportP2PSDR );
	#endif

	return true;
}

void CGameNetworkConnectionP2P::CheckInitICE()
{
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE

	// Did we already fail?
	if ( GetICEFailureCode() != 0 )
		return;

	// Already created?
	if ( m_pTransportICE )
		return;
	Assert( !m_pTransportICEPendingDelete );
	CheckCleanupICE();

	if ( IsSDRHostedServerClient() || IsSDRHostedServer() )
	{
		// Don't use ICEFailed() here.  We don't we don't want to spew and don't need anything else it does
		m_msgICESessionSummary.set_failure_reason_code( k_nICECloseCode_Local_Special );
		return;
	}

	// Fetch enabled option
	int P2P_Transport_ICE_Enable = m_connectionConfig.m_P2P_Transport_ICE_Enable.Get();
	if ( P2P_Transport_ICE_Enable < 0 )
	{

		// Ask platform if we should enable it for this peer
		int nUserFlags = -1;
		P2P_Transport_ICE_Enable = m_pGameNetworkingSocketsInterface->GetP2P_Transport_ICE_Enable( m_identityRemote, &nUserFlags );
		if ( nUserFlags >= 0 )
		{
			m_msgICESessionSummary.set_user_settings( nUserFlags );
		}
	}

	// Burn it into the connection config, if we inherited it, since we cannot change it
	// after this point.  (Note in some cases we may be running this initialization
	// for a second time, restarting ICE, so it might already be locked.)
	if ( !m_connectionConfig.m_P2P_Transport_ICE_Enable.IsLocked() )
	{
		m_connectionConfig.m_P2P_Transport_ICE_Enable.Set( P2P_Transport_ICE_Enable );
		m_connectionConfig.m_P2P_Transport_ICE_Enable.Lock();
	}

	// Disabled?
	if ( P2P_Transport_ICE_Enable <= 0 )
	{
		ICEFailed( k_nICECloseCode_Local_UserNotEnabled, "ICE not enabled by local user options" );
		return;
	}

	m_msgICESessionSummary.set_ice_enable_var( P2P_Transport_ICE_Enable );


#ifdef STEAMWEBRTC_USE_STATIC_LIBS
	g_GameNetworkingSockets_CreateICESessionFunc = (CreateICESession_t)CreateWebRTCICESession;
#else
	// No ICE factory?
	if ( !g_GameNetworkingSockets_CreateICESessionFunc )
	{
		// Just try to load up the dll directly
		static bool tried;
		if ( !tried )
		{
			GameNetworkingErrMsg errMsg;
			tried = true;
			GameNetworkingGlobalLock::SetLongLockWarningThresholdMS( "LoadICEDll", 500 );
			static const char pszExportFunc[] = "CreateWebRTCICESession";

			#if defined( _WINDOWS )
				#ifdef _WIN64
					static const char pszModule[] = "gamewebrtc64.dll";
				#else
					static const char pszModule[] = "gamewebrtc.dll";
				#endif
				HMODULE h = ::LoadLibraryA( pszModule );
				if ( h == NULL )
				{
					V_sprintf_safe( errMsg, "Failed to load %s.", pszModule ); // FIXME - error code?  Debugging DLL issues is so busted on Windows
					ICEFailed( k_nICECloseCode_Local_NotCompiled, errMsg );
					return;
				}
				g_GameNetworkingSockets_CreateICESessionFunc = (CreateICESession_t)::GetProcAddress( h, pszExportFunc );
			#elif defined( POSIX )
				#if defined( OSX ) || defined( IOS ) || defined( TVOS )
					static const char pszModule[] = "libgamewebrtc.dylib";
				#else
					static const char pszModule[] = "libgamewebrtc.so";
				#endif
				void* h = dlopen(pszModule, RTLD_LAZY);
				if ( h == NULL )
				{
					V_sprintf_safe( errMsg, "Failed to dlopen %s.  %s", pszModule, dlerror() );
					ICEFailed( k_nICECloseCode_Local_NotCompiled, errMsg );
					return;
				}
				g_GameNetworkingSockets_CreateICESessionFunc = (CreateICESession_t)dlsym( h, pszExportFunc );
			#else
				#error Need gamewebrtc for this platform
			#endif
			if ( !g_GameNetworkingSockets_CreateICESessionFunc )
			{
				V_sprintf_safe( errMsg, "%s not found in %s.", pszExportFunc, pszModule );
				ICEFailed( k_nICECloseCode_Local_NotCompiled, errMsg );
				return;
			}
		}
		if ( !g_GameNetworkingSockets_CreateICESessionFunc )
		{
			ICEFailed( k_nICECloseCode_Local_NotCompiled, "No ICE session factory" );
			return;
		}
	}
#endif

	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	// Initialize ICE.
	// WARNING: if this fails, it might set m_pTransportICE=NULL
	m_pTransportICE = new CConnectionTransportP2PICE( *this );
	m_pTransportICE->Init();

	// Process any rendezvous messages that were pended
	for ( int i = 0 ; i < len( m_vecPendingICEMessages ) && m_pTransportICE ; ++i )
		m_pTransportICE->RecvRendezvous( m_vecPendingICEMessages[i], usecNow );
	m_vecPendingICEMessages.clear();

	// If we have failed here, go ahead and cleanup now
	CheckCleanupICE();

	// If we're still all good, then add it to the list of options
	if ( m_pTransportICE )
	{
		m_vecAvailableTransports.push_back( m_pTransportICE );

		// Set a field in the ice session summary message,
		// which is how we will remember that we did attempt to use ICE
		m_msgICESessionSummary.set_local_candidate_types( 0 );
	}
#endif
}


void CGameNetworkConnectionP2P::EnsureICEFailureReasonSet( GameNetworkingMicroseconds usecNow )
{
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE

	// Already have a reason?
	if ( m_msgICESessionSummary.has_failure_reason_code() )
		return;

	// If we never tried ICE, then there's no "failure"!
	if ( !m_msgICESessionSummary.has_local_candidate_types() )
		return;

	// Classify failure, and make it permanent
	EGameNetConnectionEnd nReasonCode;
	GuessICEFailureReason( nReasonCode, m_szICECloseMsg, usecNow );
	m_msgICESessionSummary.set_failure_reason_code( nReasonCode );
	int nSeverity = ( nReasonCode != 0 && nReasonCode != k_nICECloseCode_Aborted ) ? k_EGameNetworkingSocketsDebugOutputType_Msg : k_EGameNetworkingSocketsDebugOutputType_Verbose;
	SpewTypeGroup( nSeverity, LogLevel_P2PRendezvous(), "[%s] Guessed ICE failure to be %d: %s\n",
		GetDescription(), nReasonCode, m_szICECloseMsg );

#endif
}

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
void CGameNetworkConnectionP2P::GuessICEFailureReason( EGameNetConnectionEnd &nReasonCode, ConnectionEndDebugMsg &msg, GameNetworkingMicroseconds usecNow )
{
	// Already have a reason?
	if ( m_msgICESessionSummary.failure_reason_code() )
	{
		nReasonCode = EGameNetConnectionEnd( m_msgICESessionSummary.failure_reason_code() );
		V_strcpy_safe( msg, m_szICECloseMsg );
		return;
	}

	// This should not be called if we never even tried
	Assert( m_msgICESessionSummary.has_local_candidate_types() );

	// This ought to be called before we cleanup and destroy the info we need
	Assert( m_pTransportICE );

	// If we are connected right now, then there is no problem!
	if ( m_pTransportICE && !m_pTransportICE->m_bNeedToConfirmEndToEndConnectivity )
	{
		nReasonCode = k_EGameNetConnectionEnd_Invalid;
		V_strcpy_safe( msg, "OK" );
		return;
	}

	// Did we ever pierce NAT?  If so, then we just dropped connection.
	if ( m_msgICESessionSummary.has_negotiation_ms() )
	{
		nReasonCode = k_EGameNetConnectionEnd_Misc_Timeout;
		V_strcpy_safe( msg, "ICE connection dropped after successful negotiation" );
		return;
	}

	// OK, looks like we never pierced NAT.  Try to figure out why.
	const int nAllowedTypes = m_pTransportICE ? m_pTransportICE->m_nAllowedCandidateTypes : 0;
	const int nGatheredTypes = m_msgICESessionSummary.local_candidate_types();
	const int nFailedToGatherTypes = nAllowedTypes & ~nGatheredTypes;
	const int nRemoteTypes = m_msgICESessionSummary.remote_candidate_types();

	// Terminated prematurely?  Presumably the higher level code hs a reason,
	// and so this will only be used for analytics.
	if ( m_usecWhenStartedFindingRoute == 0 || m_usecWhenStartedFindingRoute+5*k_nMillion > usecNow )
	{
		nReasonCode = EGameNetConnectionEnd( k_nICECloseCode_Aborted );
		V_strcpy_safe( msg, "NAT traversal aborted" );
		return;
	}

	// If we enabled all host candidates, and failed to gather any, then we have a problem
	// on our end.  Note that if we only allow one or the other kind, or only IPv4, etc, that
	// there are network configurations where we may legit fail to gather candidates.  (E.g.
	// their IP address is public and they don't have a LAN IP.  Or they only have IPv6.)  But
	// every computer should have *some* IP, and if we enabled all host candidate types (which
	// will be a in important use case worth handling specifically), then we should gather some
	// host candidates.
	const int k_EICECandidate_Any_Host = k_EICECandidate_Any_HostPrivate | k_EICECandidate_Any_HostPublic;
	if ( ( nFailedToGatherTypes & k_EICECandidate_Any_Host ) == k_EICECandidate_Any_Host )
	{
		// We should always be able to collect these sorts of candidates!
		nReasonCode = k_EGameNetConnectionEnd_Misc_InternalError;
		V_strcpy_safe( msg, "Never gathered *any* host candidates?" );
		return;
	}

	// Never received *any* candidates from them?
	if ( nRemoteTypes == 0 )
	{
		// FIXME - not we probably can detect if it's likely to be on their end.
		// If we are getting signals from them, just none with any candidates,
		// then it's very likely on their end, not just because they gathered
		// them but couldn't send them to us.
		nReasonCode = k_EGameNetConnectionEnd_Misc_Generic;
		V_strcpy_safe( msg, "Never received any remote candidates" );
		return;
	}

	// We failed to STUN?
	if ( ( nAllowedTypes & k_EICECandidate_Any_Reflexive ) != 0 && ( nGatheredTypes & (k_EICECandidate_Any_Reflexive|k_EICECandidate_IPv4_HostPublic) ) == 0 )
	{
		if ( m_connectionConfig.m_P2P_STUN_ServerList.Get().empty() )
		{
			nReasonCode = k_EGameNetConnectionEnd_Misc_InternalError;
			V_strcpy_safe( msg, "No configured STUN servers" );
			return;
		}
		nReasonCode = k_EGameNetConnectionEnd_Local_P2P_ICE_NoPublicAddresses;
		V_strcpy_safe( msg, "Failed to determine our public address via STUN" );
		return;
	}

	// FIXME - we should probably handle this as a special case.  TURN candidates
	// should basically always work
	//if ( (nAllowedTypes|nGatheredTypes) | k_EICECandidate_Any_Relay )
	//{
	//}

	// Any candidates from remote host that we really ought to have been able to talk to?
	if ( !(nRemoteTypes & ( k_EICECandidate_IPv4_HostPublic|k_EICECandidate_Any_Reflexive|k_EICECandidate_Any_Relay) ) )
	{
		nReasonCode = k_EGameNetConnectionEnd_Remote_P2P_ICE_NoPublicAddresses;
		V_strcpy_safe( msg, "No public or relay candidates from remote host" );
		return;
	}

	// NOTE: in theory, we could haveIPv4 vs IPv6 capabilities mismatch.  In practice
	// does that ever happen?

	// OK, both sides shared reflexive candidates, but we still failed?  This is probably
	// a firewall thing
	nReasonCode = k_EGameNetConnectionEnd_Misc_P2P_NAT_Firewall;
	V_strcpy_safe( msg, "NAT traversal failed" );
}
#endif

void CGameNetworkConnectionP2P::CheckCleanupICE()
{
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
	if ( m_pTransportICEPendingDelete )
		DestroyICENow();
#endif
}

void CGameNetworkConnectionP2P::DestroyICENow()
{
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
	AssertLocksHeldByCurrentThread( "P2P DestroyICENow" );

	// If transport was selected, then make sure and deselect, and force a re-evaluation ASAP
	if ( m_pTransport && ( m_pTransport == m_pTransportICEPendingDelete || m_pTransport == m_pTransportICE ) )
	{
		SelectTransport( nullptr, GameNetworkingSockets_GetLocalTimestamp() );
		m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
		SetNextThinkTimeASAP();
	}

	// Destroy
	if ( m_pTransportICE )
	{
		Assert( m_pTransportICE != m_pTransportICEPendingDelete );
		m_pTransportICE->TransportDestroySelfNow();
		m_pTransportICE = nullptr;
	}
	if ( m_pTransportICEPendingDelete )
	{
		m_pTransportICEPendingDelete->TransportDestroySelfNow();
		m_pTransportICEPendingDelete = nullptr;
	}

	m_vecPendingICEMessages.clear();
#endif

}

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
void CGameNetworkConnectionP2P::ICEFailed( int nReasonCode, const char *pszReason )
{
	AssertLocksHeldByCurrentThread();

	// Remember reason code, if we didn't already set one
	if ( GetICEFailureCode() == 0 )
	{
		SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] ICE failed %d %s\n", GetDescription(), nReasonCode, pszReason );
		m_msgICESessionSummary.set_failure_reason_code( nReasonCode );
		V_strcpy_safe( m_szICECloseMsg, pszReason );
	}

	// Queue for deletion
	if ( !m_pTransportICEPendingDelete )
	{
		m_pTransportICEPendingDelete = m_pTransportICE;
		m_pTransportICE = nullptr;

		// Make sure we clean ourselves up as soon as it is safe to do so
		SetNextThinkTimeASAP();
	}
}
#endif

void CGameNetworkConnectionP2P::FreeResources()
{
	AssertLocksHeldByCurrentThread();

	// Remove from global map, if we're in it
	RemoveP2PConnectionMapByRemoteInfo();

	// Release signaling
	if ( m_pSignaling )
	{	
		m_pSignaling->Release();
		m_pSignaling = nullptr;
	}

	// Base class cleanup
	CGameNetworkConnectionBase::FreeResources();
}

void CGameNetworkConnectionP2P::DestroyTransport()
{
	AssertLocksHeldByCurrentThread();

	// We're about to nuke all of our transports, don't point at any of them.
	m_pTransport = nullptr;
	m_pCurrentTransportP2P = nullptr;

	// Destroy ICE first
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		DestroyICENow();
	#endif

	// Nuke all other P2P transports
	for ( int i = len( m_vecAvailableTransports )-1 ; i >= 0 ; --i )
	{
		m_vecAvailableTransports[i]->m_pSelfAsConnectionTransport->TransportDestroySelfNow();
		Assert( len( m_vecAvailableTransports ) == i );
	}

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		Assert( m_pTransportP2PSDR == nullptr ); // Should have been nuked above

		#ifdef SDR_ENABLE_HOSTED_CLIENT
			if ( m_pTransportToSDRServer )
			{
				m_pTransportToSDRServer->TransportDestroySelfNow();
				m_pTransportToSDRServer = nullptr;
			}
		#endif

		#ifdef SDR_ENABLE_HOSTED_SERVER
			if ( m_pTransportFromSDRClient )
			{
				m_pTransportFromSDRClient->TransportDestroySelfNow();
				m_pTransportFromSDRClient = nullptr;
			}
		#endif
	#endif
}

CGameNetworkConnectionP2P *CGameNetworkConnectionP2P::FindDuplicateConnection( CGameNetworkingSockets *pInterfaceLocal, int nLocalVirtualPort, const GameNetworkingIdentity &identityRemote, int nRemoteVirtualPort, bool bOnlySymmetricConnections, CGameNetworkConnectionP2P *pIgnore )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	for ( CGameNetworkConnectionBase *pConn: g_mapConnections.IterValues() )
	{
		if ( pConn->m_pGameNetworkingSocketsInterface != pInterfaceLocal )
			continue;
		if ( !(  pConn->m_identityRemote == identityRemote ) )
			continue;

		// Check state
		switch ( pConn->GetState() )
		{
			case k_EGameNetworkingConnectionState_Dead:
			default:
				Assert( false );
			case k_EGameNetworkingConnectionState_ClosedByPeer:
			case k_EGameNetworkingConnectionState_FinWait:
			case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
				// Connection no longer alive, we could create a new one
				continue;

			case k_EGameNetworkingConnectionState_None:
				// Not finished initializing.  But that should only possibly be the case
				// for one connection, one we are in the process of creating.  And so we
				// should be ignoring it.
				Assert( pConn == pIgnore );
				continue;

			case k_EGameNetworkingConnectionState_Connecting:
			case k_EGameNetworkingConnectionState_FindingRoute:
			case k_EGameNetworkingConnectionState_Connected:
			case k_EGameNetworkingConnectionState_Linger:
				// Yes, it's a possible duplicate
				break;
		}
		if ( bOnlySymmetricConnections && !pConn->BSymmetricMode() )
			continue;
		if ( pConn == pIgnore )
			continue;
		CGameNetworkConnectionP2P *pConnP2P = pConn->AsGameNetworkConnectionP2P();
		if ( !pConnP2P )
			continue;
		if ( pConnP2P->m_nRemoteVirtualPort != nRemoteVirtualPort )
			continue;
		if ( pConnP2P->LocalVirtualPort() != nLocalVirtualPort )
			continue;
		return pConnP2P;
	}

	return nullptr;
}

EResult CGameNetworkConnectionP2P::AcceptConnection( GameNetworkingMicroseconds usecNow )
{
	AssertLocksHeldByCurrentThread( "P2P::AcceptConnection" );

	// Calling code shouldn't call us unless this is true
	Assert( m_bConnectionInitiatedRemotely );
	Assert( GetState() == k_EGameNetworkingConnectionState_Connecting );
	Assert( !IsSDRHostedServer() ); // Those connections use a derived class that overrides this function

	// Check symmetric mode.  Note that if they are using the API properly, we should
	// have already detected this earlier!
	if ( BSymmetricMode() )
	{
		if ( CGameNetworkConnectionP2P::FindDuplicateConnection( m_pGameNetworkingSocketsInterface, LocalVirtualPort(), m_identityRemote, m_nRemoteVirtualPort, false, this ) )
		{
			ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_Misc_InternalError, "Cannot accept connection, duplicate symmetric connection already exists" );
			return k_EResultFail;
		}
	}

	// Spew
	SpewVerboseGroup( LogLevel_P2PRendezvous(), "[%s] Accepting connection, transitioning to 'finding route' state\n", GetDescription() );

	// Check for enabling ICE
	CheckInitICE();

	// At this point, we should have at least one possible transport.  If not, we are sunk.
	if ( m_vecAvailableTransports.empty() && m_pTransport == nullptr )
	{

		// The only way we should be able to get here is if ICE is
		// the only transport that is enabled in this configuration,
		// and it has failed.
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
			Assert( GetICEFailureCode() != 0 );
			ConnectionState_ProblemDetectedLocally( (EGameNetConnectionEnd)GetICEFailureCode(), "%s", m_szICECloseMsg );
		#else
			Assert( false );
			ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_Misc_Generic, "No available transports?" );
		#endif
		return k_EResultFail;
	}

	// Send them a reply, and include whatever info we have right now
	SendConnectOKSignal( usecNow );

	// WE'RE NOT "CONNECTED" YET!
	// We need to do route negotiation first, which could take several route trips,
	// depending on what ping data we already had before we started.
	ConnectionState_FindingRoute( usecNow );

	// OK
	return k_EResultOK;
}

void CGameNetworkConnectionP2P::ProcessSNPPing( int msPing, RecvPacketContext_t &ctx )
{
	if ( ctx.m_pTransport == m_pTransport || m_pTransport == nullptr )
		CGameNetworkConnectionBase::ProcessSNPPing( msPing, ctx );

	// !KLUDGE! Because we cannot upcast.  This list should be short, though
	for ( CConnectionTransportP2PBase *pTransportP2P: m_vecAvailableTransports )
	{
		if ( pTransportP2P->m_pSelfAsConnectionTransport == ctx.m_pTransport )
		{
			pTransportP2P->m_pingEndToEnd.ReceivedPing( msPing, ctx.m_usecNow );
		}
	}
}

bool CGameNetworkConnectionP2P::BSupportsSymmetricMode()
{
	return true;
}

void CGameNetworkConnectionP2P::TransportEndToEndConnectivityChanged( CConnectionTransportP2PBase *pTransport, GameNetworkingMicroseconds usecNow )
{
	AssertLocksHeldByCurrentThread( "P2P::TransportEndToEndConnectivityChanged" );

	if ( pTransport->m_bNeedToConfirmEndToEndConnectivity == ( pTransport == m_pCurrentTransportP2P ) )
	{
		// Connectivity was lost on the current transport, gained on a transport not currently selected.
		// This is an event that should cause us to take action
		// Clear any stickiness to current transport, and schedule us to wake up
		// immediately and re-evaluate the situation
		m_bTransportSticky = false;
		m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
	}

	// Reset counter to make sure we collect a few more, either immediately if we can, or when
	// we come back alive.  Also, this makes sure that as soon as we get confirmed connectivity,
	// that we send something to the peer so they can get confirmation, too.
	pTransport->m_nKeepTryingToPingCounter = std::max( pTransport->m_nKeepTryingToPingCounter, 5 );

	// Wake up the connection immediately, either to evaluate transports, or to send packets
	SetNextThinkTimeASAP();

	// Check for recording the time when a transport first became available
	if ( !pTransport->m_bNeedToConfirmEndToEndConnectivity && BStateIsActive() )
	{

		GameNetworkingMicroseconds usecWhenStartedNegotiation = m_usecWhenStartedFindingRoute;
		if ( usecWhenStartedNegotiation == 0 )
		{
			// It's actually possible for us to confirm end-to-end connectivity before
			// entering the routing finding state.  If we are initiating the connection,
			// we might have sent info to the peer through our connect request which they
			// use to get back to us over the transport, before their COnnectOK reply signal
			// reaches us!
			Assert( GetState() == k_EGameNetworkingConnectionState_Connecting );
			usecWhenStartedNegotiation = GetTimeEnteredConnectionState();
		}

		// Round to nearest ms, and clamp to 1, to make sure that 0 is not interpreted
		// anywhere as "no data", instead of "incredibly fast", which is actually happening.
		int msNegotiationTime = std::max( 1, (int)( ( usecNow - usecWhenStartedNegotiation + 500 ) / 1000 ) );

		// Which transport?
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
			if ( pTransport == m_pTransportICE && !m_msgICESessionSummary.has_negotiation_ms() )
				m_msgICESessionSummary.set_negotiation_ms( msNegotiationTime );
		#endif
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
			if ( pTransport == m_pTransportP2PSDR && !m_msgSDRRoutingSummary.has_negotiation_ms() )
				m_msgSDRRoutingSummary.set_negotiation_ms( msNegotiationTime );
		#endif

		// Compiler warning if nothing enabled
		(void)msNegotiationTime;
	}
}

void CGameNetworkConnectionP2P::ConnectionStateChanged( EGameNetworkingConnectionState eOldState )
{
	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	// NOTE: Do not call base class, because it it going to
	// call TransportConnectionStateChanged on whatever transport is active.
	// We don't want that here.

	// Take action at certain transitions
	switch ( GetState() )
	{
		case k_EGameNetworkingConnectionState_Dead:
		case k_EGameNetworkingConnectionState_None:
		default:
			Assert( false );
			break;

		case k_EGameNetworkingConnectionState_ClosedByPeer:
		case k_EGameNetworkingConnectionState_FinWait:
			EnsureICEFailureReasonSet( usecNow );
			break;

		case k_EGameNetworkingConnectionState_Linger:
			break;

		case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
			EnsureICEFailureReasonSet( usecNow );

			// If we fail during these states, send a signal to Steam, for analytics
			if ( eOldState == k_EGameNetworkingConnectionState_Connecting || eOldState == k_EGameNetworkingConnectionState_FindingRoute )
				SendConnectionClosedSignal( usecNow );
			break;

		case k_EGameNetworkingConnectionState_FindingRoute:
			Assert( m_usecWhenStartedFindingRoute == 0 ); // Should only enter this state once
			m_usecWhenStartedFindingRoute = usecNow;
			// |
			// |
			// V
			// FALLTHROUGH
		case k_EGameNetworkingConnectionState_Connecting:
			m_bTransportSticky = false; // Not sure how we could have set this flag, but make sure and clear it
			// |
			// |
			// V
			// FALLTHROUGH
		case k_EGameNetworkingConnectionState_Connected:

			// Kick off thinking loop, perhaps taking action immediately
			m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
			SetNextThinkTimeASAP();
			for ( CConnectionTransportP2PBase *pTransportP2P: m_vecAvailableTransports )
				pTransportP2P->EnsureP2PTransportThink( k_nThinkTime_ASAP );

			break;
	}

	// Inform transports
	for ( CConnectionTransportP2PBase *pTransportP2P: m_vecAvailableTransports )
		pTransportP2P->m_pSelfAsConnectionTransport->TransportConnectionStateChanged( eOldState );
}

void CGameNetworkConnectionP2P::ThinkConnection( GameNetworkingMicroseconds usecNow )
{
	CGameNetworkConnectionBase::ThinkConnection( usecNow );

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		CheckCleanupICE();
	#endif

	// Check for sending signals pending for RTO or Nagle.
	// (If we have gotten far enough along where we know where
	// to send them.  Some messages can be queued very early, and
	// do not depend on who the peer it.)
	if ( GetState() != k_EGameNetworkingConnectionState_Connecting )
	{

		// Process route selection
		ThinkSelectTransport( usecNow );

		// If nothing scheduled, check RTOs.  If we have something scheduled,
		// wait for the timer. The timer is short and designed to avoid
		// a blast, so let it do its job.
		if ( m_usecSendSignalDeadline == k_nThinkTime_Never )
		{
			for ( const OutboundMessage &s: m_vecUnackedOutboundMessages )
			{
				if ( s.m_usecRTO < m_usecSendSignalDeadline )
				{
					m_usecSendSignalDeadline = s.m_usecRTO;
					m_pszNeedToSendSignalReason = "MessageRTO";
					// Keep scanning the list.  we want to collect
					// the minimum RTO.
				}
			}
		}

		if ( usecNow >= m_usecSendSignalDeadline )
		{
			Assert( m_pszNeedToSendSignalReason );

			// Send a signal
			CMsgGameNetworkingP2PRendezvous msgRendezvous;
			SetRendezvousCommonFieldsAndSendSignal( msgRendezvous, usecNow, m_pszNeedToSendSignalReason );
		}

		Assert( m_usecSendSignalDeadline > usecNow );

		EnsureMinThinkTime( m_usecSendSignalDeadline );
	}
}

void CGameNetworkConnectionP2P::ThinkSelectTransport( GameNetworkingMicroseconds usecNow )
{

	// If no available transports, then nothing to think about.
	// (This will be the case if we are using a special transport type that isn't P2P.)
	if ( m_vecAvailableTransports.empty() )
	{
		m_usecNextEvaluateTransport = k_nThinkTime_Never;
		m_bTransportSticky = true;
		return;
	}

	// Time to evaluate which transport to use?
	if ( usecNow < m_usecNextEvaluateTransport )
	{
		EnsureMinThinkTime( m_usecNextEvaluateTransport );
		return;
	}

	AssertLocksHeldByCurrentThread( "P2P::ThinkSelectTRansport" );

	// Reset timer to evaluate transport at certain times
	switch ( GetState() )
	{
		case k_EGameNetworkingConnectionState_Dead:
		case k_EGameNetworkingConnectionState_None:
		default:
			Assert( false );
			// FALLTHROUGH

		case k_EGameNetworkingConnectionState_ClosedByPeer:
		case k_EGameNetworkingConnectionState_FinWait:
		case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
		case k_EGameNetworkingConnectionState_Connecting: // wait for signaling to complete
			m_usecNextEvaluateTransport = k_nThinkTime_Never;
			return;

		case k_EGameNetworkingConnectionState_Linger:
		case k_EGameNetworkingConnectionState_Connected:
		case k_EGameNetworkingConnectionState_FindingRoute:
			m_usecNextEvaluateTransport = usecNow + k_nMillion; // Check back periodically
			break;
	}

	bool bEvaluateFrequently = false;

	// Make sure extreme penalty numbers make sense
	constexpr int k_nMaxReasonableScore = k_nRoutePenaltyNeedToConfirmConnectivity + k_nRoutePenaltyNotNominated + k_nRoutePenaltyNotSelectedOverride + 2000;
	COMPILE_TIME_ASSERT( k_nMaxReasonableScore >= 0 && k_nMaxReasonableScore*2 < k_nRouteScoreHuge/2 );

	// Scan all the options
	int nCurrentTransportScore = k_nRouteScoreHuge;
	int nBestTransportScore = k_nRouteScoreHuge;
	CConnectionTransportP2PBase *pBestTransport = nullptr;
	for ( CConnectionTransportP2PBase *t: m_vecAvailableTransports )
	{
		// Update metrics
		t->P2PTransportUpdateRouteMetrics( usecNow );

		// Add on a penalty if we need to confirm connectivity
		if ( t->m_bNeedToConfirmEndToEndConnectivity )
			t->m_routeMetrics.m_nTotalPenalty += k_nRoutePenaltyNeedToConfirmConnectivity;

		// If we are the controlled agent, add a penalty to non-nominated transports
		if ( !IsControllingAgent() && m_pPeerSelectedTransport != t )
			t->m_routeMetrics.m_nTotalPenalty += k_nRoutePenaltyNotNominated;

		// Calculate the total score
		int nScore = t->m_routeMetrics.m_nScoreCurrent + t->m_routeMetrics.m_nTotalPenalty;
		if ( t == m_pCurrentTransportP2P )
			nCurrentTransportScore = nScore;
		if ( nScore < nBestTransportScore )
		{
			nBestTransportScore = nScore;
			pBestTransport = t;
		}

		// Should not be using the special "force select this transport" score
		// if we have more than one transport
		Assert( nScore >= 0 || m_vecAvailableTransports.size() == 1 );
	}

	if ( pBestTransport == nullptr )
	{
		// No suitable transports at all?
		SelectTransport( nullptr, usecNow );
	}
	else if ( len( m_vecAvailableTransports ) == 1 )
	{
		// We only have one option.  No use waiting
		SelectTransport( pBestTransport, usecNow );
		m_bTransportSticky = true;
	}
	else if ( pBestTransport->m_bNeedToConfirmEndToEndConnectivity )
	{
		// Don't switch or activate a transport if we are not certain
		// about its connectivity and we might have other options
		m_bTransportSticky = false;
	}
	else if ( m_pCurrentTransportP2P == nullptr )
	{
		m_bTransportSticky = false;

		// We're making the initial decision, or we lost all transports.
		// If we're not the controlling agent, give the controlling agent
		// a bit of time
		if (
			IsControllingAgent() // we're in charge
			|| m_pPeerSelectedTransport == pBestTransport // we want to switch to what the other guy said
			|| GetTimeEnteredConnectionState() + k_usecWaitForControllingAgentBeforeSelectingNonNominatedTransport < usecNow // we've waited long enough
		) {

			// Select something as soon as it becomes available
			SelectTransport( pBestTransport, usecNow );
		}
		else
		{
			// Wait for the controlling agent to make a decision
			bEvaluateFrequently = true;
		}
	}
	else if ( m_pCurrentTransportP2P != pBestTransport )
	{

		const auto &GetStickyPenalizedScore = []( int nScore ) { return nScore * 11 / 10 + 5; };

		// Check for applying a sticky penalty, that the new guy has to
		// overcome to switch
		int nBestScoreWithStickyPenalty = nBestTransportScore;
		if ( m_bTransportSticky )
			nBestScoreWithStickyPenalty = GetStickyPenalizedScore( nBestTransportScore );

		// Still better?
		if ( nBestScoreWithStickyPenalty < nCurrentTransportScore )
		{

			// Make sure we have enough recent ping data to make
			// the switch
			bool bReadyToSwitch = true;
			if ( m_bTransportSticky )
			{

				// We don't have a particular reason to switch, so let's make sure the new option is
				// consistently better than the current option, over a sustained time interval
				if (
					GetStickyPenalizedScore( pBestTransport->m_routeMetrics.m_nScoreMax ) + pBestTransport->m_routeMetrics.m_nTotalPenalty
					 < m_pCurrentTransportP2P->m_routeMetrics.m_nScoreMin + m_pCurrentTransportP2P->m_routeMetrics.m_nTotalPenalty
				) {
					bEvaluateFrequently = true;

					// The new transport is consistently better within all recent samples.  But is that just because
					// we don't have many samples?  If so, let's make sure and collect some
					#define CHECK_READY_TO_SWITCH( pTransport ) \
						if ( pTransport->m_routeMetrics.m_nBucketsValid < k_nRecentValidTimeBucketsToSwitchRoute ) \
						{ \
							bReadyToSwitch = false; \
							GameNetworkingMicroseconds usecNextPing = pTransport->m_pingEndToEnd.TimeToSendNextAntiFlapRouteCheckPingRequest(); \
							if ( usecNextPing > usecNow ) \
							{ \
								m_usecNextEvaluateTransport = std::min( m_usecNextEvaluateTransport, usecNextPing ); \
							} \
							else if ( pTransport->m_usecEndToEndInFlightReplyTimeout > 0 ) \
							{ \
								m_usecNextEvaluateTransport = std::min( m_usecNextEvaluateTransport, pTransport->m_usecEndToEndInFlightReplyTimeout ); \
							} \
							else \
							{ \
								SpewVerbose( "[%s] %s (%d+%d) appears preferable to current transport %s (%d+%d), but maybe transient.  Pinging via %s.", \
									GetDescription(), \
									pBestTransport->m_pszP2PTransportDebugName, \
									pBestTransport->m_routeMetrics.m_nScoreCurrent, pBestTransport->m_routeMetrics.m_nTotalPenalty, \
									m_pCurrentTransportP2P->m_pszP2PTransportDebugName, \
									m_pCurrentTransportP2P->m_routeMetrics.m_nScoreCurrent, m_pCurrentTransportP2P->m_routeMetrics.m_nTotalPenalty, \
									pTransport->m_pszP2PTransportDebugName \
								); \
								pTransport->m_pSelfAsConnectionTransport->SendEndToEndStatsMsg( k_EStatsReplyRequest_Immediate, usecNow, "TransportChangeConfirm" ); \
							} \
						}

					CHECK_READY_TO_SWITCH( pBestTransport )
					CHECK_READY_TO_SWITCH( m_pCurrentTransportP2P )

					#undef CHECK_READY_TO_SWITCH
				}
			}

			if ( bReadyToSwitch )
				SelectTransport( pBestTransport, usecNow );
			else
				bEvaluateFrequently = true;
		}
	}

	// Check for turning on the sticky flag if things look solid
	if (
		m_pCurrentTransportP2P
		&& m_pCurrentTransportP2P == pBestTransport
		&& !m_pCurrentTransportP2P->m_bNeedToConfirmEndToEndConnectivity // Never be sticky do a transport that we aren't sure we can communicate on!
		&& ( IsControllingAgent() || m_pPeerSelectedTransport == m_pCurrentTransportP2P ) // Don't be sticky to a non-nominated transport
	) {
		m_bTransportSticky = true;
	}

	// As soon as we have any viable transport, exit route finding.
	if ( GetState() == k_EGameNetworkingConnectionState_FindingRoute )
	{
		if ( m_pCurrentTransportP2P && !m_pCurrentTransportP2P->m_bNeedToConfirmEndToEndConnectivity )
		{
			ConnectionState_Connected( usecNow );
		}
		else
		{
			bEvaluateFrequently = true;
		}
	}

	// If we're not settled, then make sure we're checking in more frequently
	if ( bEvaluateFrequently || !m_bTransportSticky || m_pCurrentTransportP2P == nullptr || pBestTransport == nullptr || m_pCurrentTransportP2P->m_bNeedToConfirmEndToEndConnectivity || pBestTransport->m_bNeedToConfirmEndToEndConnectivity )
		m_usecNextEvaluateTransport = std::min( m_usecNextEvaluateTransport, usecNow + k_nMillion/20 );

	EnsureMinThinkTime( m_usecNextEvaluateTransport );
}

void CGameNetworkConnectionP2P::SelectTransport( CConnectionTransportP2PBase *pTransportP2P, GameNetworkingMicroseconds usecNow )
{
	CConnectionTransport *pTransport = pTransportP2P ? pTransportP2P->m_pSelfAsConnectionTransport : nullptr;

	// No change?
	if ( pTransportP2P == m_pCurrentTransportP2P )
	{
		return;
	}

	AssertLocksHeldByCurrentThread( "P2P::SelectTransport" );

	// Spew about this event
	const int nLogLevel = LogLevel_P2PRendezvous();
	if ( nLogLevel >= k_EGameNetworkingSocketsDebugOutputType_Verbose )
	{
		if ( pTransportP2P == nullptr )
		{
			if ( BStateIsActive() ) // Don't spew about cleaning up
				ReallySpewTypeFmt( nLogLevel, "[%s] Deselected '%s' transport, no transport currently active!\n", GetDescription(), m_pCurrentTransportP2P->m_pszP2PTransportDebugName );
		}
		else if ( m_pCurrentTransportP2P == nullptr )
		{
			ReallySpewTypeFmt( nLogLevel, "[%s] Selected '%s' transport (ping=%d, score=%d+%d)\n", GetDescription(),
				pTransportP2P->m_pszP2PTransportDebugName, pTransportP2P->m_pingEndToEnd.m_nSmoothedPing, pTransportP2P->m_routeMetrics.m_nScoreCurrent, pTransportP2P->m_routeMetrics.m_nTotalPenalty );
		}
		else
		{
			ReallySpewTypeFmt( nLogLevel, "[%s] Switched to '%s' transport (ping=%d, score=%d+%d) from '%s' (ping=%d, score=%d+%d)\n", GetDescription(),
				pTransportP2P->m_pszP2PTransportDebugName, pTransportP2P->m_pingEndToEnd.m_nSmoothedPing, pTransportP2P->m_routeMetrics.m_nScoreCurrent, pTransportP2P->m_routeMetrics.m_nTotalPenalty,
				m_pCurrentTransportP2P->m_pszP2PTransportDebugName, m_pCurrentTransportP2P->m_pingEndToEnd.m_nSmoothedPing, m_pCurrentTransportP2P->m_routeMetrics.m_nScoreCurrent, m_pCurrentTransportP2P->m_routeMetrics.m_nTotalPenalty
			);
		}
	}

	// Slam the connection end-to-end ping data with values from the new transport
	if ( m_pCurrentTransportP2P )
	{
		static_cast< PingTracker &>( m_statsEndToEnd.m_ping ) = static_cast< const PingTracker &>( m_pCurrentTransportP2P->m_pingEndToEnd ); // Slice cast
		m_statsEndToEnd.m_ping.m_usecTimeLastSentPingRequest = 0;

		// Count up time we were selected
		Assert( m_pCurrentTransportP2P->m_usecWhenSelected );
		m_pCurrentTransportP2P->m_usecTimeSelectedAccumulator = m_pCurrentTransportP2P->CalcTotalTimeSelected( usecNow );
		m_pCurrentTransportP2P->m_usecWhenSelected = 0;
	}

	m_pCurrentTransportP2P = pTransportP2P;
	m_pTransport = pTransport;
	if ( m_pCurrentTransportP2P && len( m_vecAvailableTransports ) == 1 )
	{
		// Only one transport.  Might as well be sticky, and no use evaluating other transports
		m_bTransportSticky = true;
		m_usecNextEvaluateTransport = k_nThinkTime_Never;
	}
	else
	{
		// Assume we won't be sticky for now
		m_bTransportSticky = false;
		m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
	}

	SetDescription();
	SetNextThinkTimeASAP(); // we might want to send packets ASAP

	// Remember when we became active
	if ( m_pCurrentTransportP2P )
	{
		Assert( m_pCurrentTransportP2P->m_usecWhenSelected == 0 );
		m_pCurrentTransportP2P->m_usecWhenSelected = usecNow;
	}

	// Make sure the summaries are updated with the current total time selected
	UpdateTransportSummaries( usecNow );

	// If we're the controlling agent, then send something on this transport ASAP
	if ( m_pTransport && IsControllingAgent() && !IsSDRHostedServerClient() )
	{
		m_pTransport->SendEndToEndStatsMsg( k_EStatsReplyRequest_NoReply, usecNow, "P2PNominate" );
	}
}

void CGameNetworkConnectionP2P::UpdateTransportSummaries( GameNetworkingMicroseconds usecNow )
{

	#define UPDATE_SECONDS_SELECTED( pTransport, msg ) \
		if ( pTransport ) \
		{ \
			GameNetworkingMicroseconds usec = pTransport->CalcTotalTimeSelected( usecNow ); \
			msg.set_selected_seconds( usec <= 0 ? 0 : std::max( 1, (int)( ( usec + 500*1000 ) / k_nMillion ) ) ); \
		}

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		UPDATE_SECONDS_SELECTED( m_pTransportICE, m_msgICESessionSummary )
	#endif

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		UPDATE_SECONDS_SELECTED( m_pTransportP2PSDR, m_msgSDRRoutingSummary )
	#endif

	#undef UPDATE_SECONDS_SELECTED
}

GameNetworkingMicroseconds CGameNetworkConnectionP2P::ThinkConnection_ClientConnecting( GameNetworkingMicroseconds usecNow )
{
	Assert( !m_bConnectionInitiatedRemotely );
	Assert( m_pParentListenSocket == nullptr );

	// FIXME if we have LAN broadcast enabled, we should send those here.
	// (Do we even need crypto ready for that, if we are gonna allow them to
	// be unauthenticated anyway?)  If so, we will need to refactor the base
	// class to call this even if crypt is not ready.

	// SDR client to hosted dedicated server?  We don't use signaling to make these connect requests.
	if ( IsSDRHostedServerClient() )
	{

		// Base class behaviour, which uses the transport to send end-to-end connect
		// requests, is the right thing to do
		return CGameNetworkConnectionBase::ThinkConnection_ClientConnecting( usecNow );
	}

	// No signaling?  This should only be possible if we are attempting P2P though LAN
	// broadcast only.
	if ( !m_pSignaling )
	{
		// LAN broadcasts not implemented, so this should currently not be possible.
		AssertMsg( false, "No signaling?" );
		return k_nThinkTime_Never;
	}

	// If we are using SDR, then we want to wait until we have finished the initial ping probes.
	// This makes sure out initial connect message doesn't contain potentially inaccurate
	// routing information.  This delay should only happen very soon after initializing the
	// relay network.
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		if ( m_pTransportP2PSDR )
		{
			if ( !m_pTransportP2PSDR->BReady() )
				return usecNow + k_nMillion/20;
		}
	#endif

	// When using ICE, it takes just a few milliseconds to collect the local candidates.
	// We'd like to send those in the initial connect request
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		if ( m_pTransportICE )
		{
			GameNetworkingMicroseconds usecWaitForICE = GetTimeEnteredConnectionState() + 5*1000;
			if ( usecNow < usecWaitForICE )
				return usecWaitForICE;
		}
	#endif

	// Time to send another connect request?
	// We always do this through signaling service rendezvous message.  We don't need to have
	// selected the transport (yet)
	GameNetworkingMicroseconds usecRetry = m_usecWhenSentConnectRequest + k_usecConnectRetryInterval;
	if ( usecNow < usecRetry )
		return usecRetry;

	// Fill out the rendezvous message
	CMsgGameNetworkingP2PRendezvous msgRendezvous;
	CMsgGameNetworkingP2PRendezvous_ConnectRequest &msgConnectRequest = *msgRendezvous.mutable_connect_request();
	*msgConnectRequest.mutable_cert() = m_msgSignedCertLocal;
	*msgConnectRequest.mutable_crypt() = m_msgSignedCryptLocal;
	msgConnectRequest.set_to_virtual_port( m_nRemoteVirtualPort );
	msgConnectRequest.set_from_virtual_port( LocalVirtualPort() );

	// Send through signaling service
	SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] Sending P2P ConnectRequest\n", GetDescription() );
	SetRendezvousCommonFieldsAndSendSignal( msgRendezvous, usecNow, "ConnectRequest" );

	// Remember when we send it
	m_usecWhenSentConnectRequest = usecNow;

	// And set timeout for retry
	return m_usecWhenSentConnectRequest + k_usecConnectRetryInterval;
}

void CGameNetworkConnectionP2P::SendConnectOKSignal( GameNetworkingMicroseconds usecNow )
{
	Assert( BCryptKeysValid() );

	CMsgGameNetworkingP2PRendezvous msgRendezvous;
	CMsgGameNetworkingP2PRendezvous_ConnectOK &msgConnectOK = *msgRendezvous.mutable_connect_ok();
	*msgConnectOK.mutable_cert() = m_msgSignedCertLocal;
	*msgConnectOK.mutable_crypt() = m_msgSignedCryptLocal;
	SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] Sending P2P ConnectOK via Steam, remote cxn %u\n", GetDescription(), m_unConnectionIDRemote );
	SetRendezvousCommonFieldsAndSendSignal( msgRendezvous, usecNow, "ConnectOK" );
}

void CGameNetworkConnectionP2P::SendConnectionClosedSignal( GameNetworkingMicroseconds usecNow )
{
	SpewVerboseGroup( LogLevel_P2PRendezvous(), "[%s] Sending graceful P2P ConnectionClosed, remote cxn %u\n", GetDescription(), m_unConnectionIDRemote );

	CMsgGameNetworkingP2PRendezvous msgRendezvous;
	CMsgGameNetworkingP2PRendezvous_ConnectionClosed &msgConnectionClosed = *msgRendezvous.mutable_connection_closed();
	msgConnectionClosed.set_reason_code( m_eEndReason );
	msgConnectionClosed.set_debug( m_szEndDebug );

	// NOTE: Not sending connection stats here.  Usually when a connection is closed through this mechanism,
	// it is because we have not been able to rendezvous, and haven't sent any packets end-to-end anyway

	SetRendezvousCommonFieldsAndSendSignal( msgRendezvous, usecNow, "ConnectionClosed" );
}

void CGameNetworkConnectionP2P::SendNoConnectionSignal( GameNetworkingMicroseconds usecNow )
{
	SpewVerboseGroup( LogLevel_P2PRendezvous(), "[%s] Sending P2P NoConnection signal, remote cxn %u\n", GetDescription(), m_unConnectionIDRemote );

	CMsgGameNetworkingP2PRendezvous msgRendezvous;
	CMsgGameNetworkingP2PRendezvous_ConnectionClosed &msgConnectionClosed = *msgRendezvous.mutable_connection_closed();
	msgConnectionClosed.set_reason_code( k_EGameNetConnectionEnd_Internal_P2PNoConnection ); // Special reason code that means "do not reply"

	// NOTE: Not sending connection stats here.  Usually when a connection is closed through this mechanism,
	// it is because we have not been able to rendezvous, and haven't sent any packets end-to-end anyway

	SetRendezvousCommonFieldsAndSendSignal( msgRendezvous, usecNow, "NoConnection" );
}

void CGameNetworkConnectionP2P::SetRendezvousCommonFieldsAndSendSignal( CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow, const char *pszDebugReason )
{
	if ( !m_pSignaling )
		return;

	AssertLocksHeldByCurrentThread( "P2P::SetRendezvousCommonFieldsAndSendSignal" );

	Assert( !msg.has_to_connection_id() );
	if ( !msg.has_connect_request() )
	{
		if ( m_unConnectionIDRemote )
		{
			msg.set_to_connection_id( m_unConnectionIDRemote );
		}
		else
		{
			Assert( msg.has_connection_closed() );
		}
	}

	char szTempIdentity[ GameNetworkingIdentity::k_cchMaxString ];
	if ( !m_identityRemote.IsInvalid() )
	{
		m_identityRemote.ToString( szTempIdentity, sizeof(szTempIdentity) );
		msg.set_to_identity( szTempIdentity );
	}
	m_identityLocal.ToString( szTempIdentity, sizeof(szTempIdentity) );
	msg.set_from_identity( szTempIdentity );
	msg.set_from_connection_id( m_unConnectionIDLocal );

	// Asks transport(s) to put routing info into the message
	PopulateRendezvousMsgWithTransportInfo( msg, usecNow );

	m_pszNeedToSendSignalReason = nullptr;
	m_usecSendSignalDeadline = INT64_MAX;

	// Reliable messages?
	if ( msg.has_connection_closed() )
	{
		// Once connection is closed, discard these, never send again
		m_vecUnackedOutboundMessages.clear();
	}
	else
	{
		bool bInitialHandshake = msg.has_connect_request() || msg.has_connect_ok();

		int nTotalMsgSize = 0;
		for ( OutboundMessage &s: m_vecUnackedOutboundMessages )
		{

			// Not yet ready to retry sending?
			if ( !msg.has_first_reliable_msg() )
			{

				// If we have sent recently, assume it's in flight,
				// and don't give up yet.  Just go ahead and move onto
				// the next once, speculatively sending them before
				// we get our ack for the previously sent ones.
				if ( s.m_usecRTO > usecNow )
				{
					if ( !bInitialHandshake ) // However, always start from the beginning in initial handshake packets
						continue;
				}

				// Try to keep individual signals relatively small.  If we have a lot
				// to say, break it up into multiple messages
				if ( nTotalMsgSize > 800 )
				{
					if ( !msg.has_connect_request() )
						ScheduleSendSignal( "ContinueLargeSignal" );
					break;
				}

				// Start sending from this guy forward
				msg.set_first_reliable_msg( s.m_nID );
			}

			*msg.add_reliable_messages() = s.m_msg;
			nTotalMsgSize += s.m_cbSerialized;

			s.m_usecRTO = usecNow + k_nMillion/2; // Reset RTO
		}

		// Go ahead and always ack, even if we don't need to, because this is small
		msg.set_ack_reliable_msg( m_nLastRecvRendesvousMessageID );
	}

	// Spew
	int nLogLevel = LogLevel_P2PRendezvous();
	SpewVerboseGroup( nLogLevel, "[%s] Sending P2PRendezvous (%s)\n", GetDescription(), pszDebugReason );
	SpewDebugGroup( nLogLevel, "%s\n\n", Indent( msg.DebugString() ).c_str() );

	int cbMsg = ProtoMsgByteSize( msg );
	uint8 *pMsg = (uint8 *)alloca( cbMsg );
	DbgVerify( msg.SerializeWithCachedSizesToArray( pMsg ) == pMsg+cbMsg );

	// Get connection info to pass to the signal sender
	GameNetConnectionInfo_t info;
	ConnectionPopulateInfo( info );

	// Send it
	if ( !m_pSignaling->SendSignal( m_hConnectionSelf, info, pMsg, cbMsg ) )
	{
		// NOTE: we might already be closed, either before this call,
		//       or the caller might have closed us!
		ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_Misc_InternalError, "Failed to send P2P signal" );
	}
}

void CGameNetworkConnectionP2P::PopulateRendezvousMsgWithTransportInfo( CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow )
{

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		if ( m_pTransportP2PSDR )
			m_pTransportP2PSDR->PopulateRendezvousMsg( msg, usecNow );
	#endif
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		if ( m_pTransportICE )
			m_pTransportICE->PopulateRendezvousMsg( msg, usecNow );
	#endif
}

bool CGameNetworkConnectionP2P::ProcessSignal( const CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow )
{
	AssertLocksHeldByCurrentThread( "P2P::ProcessSignal" );

	// SDR routing?
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

		// Check for SDR hosted server telling us to contact them via the special protocol
		if ( msg.has_hosted_server_ticket() )
		{
			#ifdef SDR_ENABLE_HOSTED_CLIENT
				if ( !IsSDRHostedServerClient() )
				{
					SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] Peer sent hosted_server_ticket.  Switching to SDR client transport\n", GetDescription() );
					if ( !BSelectTransportToSDRServerFromSignal( msg ) )
						return false;
				}
			#else
				ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_Misc_P2P_Rendezvous, "Peer is a hosted dedicated server.  Not supported." );
				return false;
			#endif
		}

		// Go ahead and process the SDR P2P routes, if they are sending them
		if ( m_pTransportP2PSDR )
		{
			if ( msg.has_sdr_routes() )
				m_pTransportP2PSDR->RecvRoutes( msg.sdr_routes() );
			m_pTransportP2PSDR->CheckRecvRoutesAck( msg );
		}
	#endif

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		if ( !msg.ice_enabled() )
		{
			ICEFailed( k_nICECloseCode_Remote_NotEnabled, "Peer sent signal without ice_enabled set" );

			// An old peer doesn't understand how to ack our messages, so nuke them.
			// note that for a newer peer, we keep them in the queue, even though this is
			// useless.  That's because they are "reliable" messages, and we don't want
			// to add a complication of trying to remove "reliable" messages that cannot
			// be acked.  (Although we could make the optimization to empty them, since we
			// know the peer would discard them.)  At the time of this writing, old peers
			// do not even understand the concept of this reliable message queue, and
			// ICE messages are the only thing that uses it, and so clearing this makes sense.
			// For protocol version 10, we know that this field is ALWAYS set in every signal
			// other than ConnectionClosed.  But we don't want to make any commitments beyond
			// version 10.  (Maybe we want to be able to stop acking after a certain point.)
			if ( !msg.has_ack_reliable_msg() && m_statsEndToEnd.m_nPeerProtocolVersion < 10 )
			{
				Assert( m_nLastRecvRendesvousMessageID == 0 );
				Assert( m_nLastSendRendesvousMessageID == m_vecUnackedOutboundMessages.size() );
				m_vecUnackedOutboundMessages.clear();
				m_nLastSendRendesvousMessageID = 0;
			}
		}
	#endif

	// Closing the connection through rendezvous?
	// (Usually we try to close the connection through the
	// data transport, but in some cases that may not be possible.)
	if ( msg.has_connection_closed() )
	{
		const CMsgGameNetworkingP2PRendezvous_ConnectionClosed &connection_closed = msg.connection_closed();

		// Give them a reply if appropriate
		if ( connection_closed.reason_code() != k_EGameNetConnectionEnd_Internal_P2PNoConnection )
			SendNoConnectionSignal( usecNow );

		// Generic state machine take it from here.  (This call does the right
		// thing regardless of the current connection state.)
		if ( connection_closed.reason_code() == k_EGameNetConnectionEnd_Internal_P2PNoConnection )
		{
			// If we were already closed, this won't actually be "unexpected".  The
			// error message and code we pass here are only used if we are not already
			// closed.
			ConnectionState_ClosedByPeer( k_EGameNetConnectionEnd_Misc_PeerSentNoConnection, "Received unexpected P2P 'no connection' signal" ); 
		}
		else
		{
			ConnectionState_ClosedByPeer( connection_closed.reason_code(), connection_closed.debug().c_str() ); 
		}
		return true;
	}

	// Check for acking reliable messages
	if ( msg.ack_reliable_msg() > 0 )
	{

		// Remove messages that are being acked
		while ( !m_vecUnackedOutboundMessages.empty() && m_vecUnackedOutboundMessages[0].m_nID <= msg.ack_reliable_msg() )
			erase_at( m_vecUnackedOutboundMessages, 0 );

		// If anything ready to retry now, schedule wakeup
		if ( m_usecSendSignalDeadline == k_nThinkTime_Never )
		{
			GameNetworkingMicroseconds usecNextRTO = k_nThinkTime_Never;
			for ( const OutboundMessage &s: m_vecUnackedOutboundMessages )
				usecNextRTO = std::min( usecNextRTO, s.m_usecRTO );
			EnsureMinThinkTime( usecNextRTO );
		}
	}

	// Check if they sent reliable messages
	if ( msg.has_first_reliable_msg() )
	{

		// Send an ack, no matter what
		ScheduleSendSignal( "AckMessages" );

		// Do we have a gap?
		if ( msg.first_reliable_msg() > m_nLastRecvRendesvousMessageID+1 )
		{
			// Something got dropped.  They will need to re-transmit.
			// FIXME We could save these, though, so that if they
			// retransmit, but not everything here, we won't have to ask them
			// for these messages again.  Just discard for now
		}
		else
		{

			// Take the update
			for ( int i = m_nLastRecvRendesvousMessageID+1-msg.first_reliable_msg() ; i < msg.reliable_messages_size() ; ++i )
			{
				++m_nLastRecvRendesvousMessageID;
				const CMsgGameNetworkingP2PRendezvous_ReliableMessage &reliable_msg = msg.reliable_messages(i);

				#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
					if ( reliable_msg.has_ice() )
					{
						if ( m_pTransportICE )
						{
							m_pTransportICE->RecvRendezvous( reliable_msg.ice(), usecNow );
						}
						else if ( GetState() == k_EGameNetworkingConnectionState_Connecting && GetICEFailureCode() == 0 )
						{
							m_vecPendingICEMessages.push_back( reliable_msg.ice() );
						}
					}
				#endif

				(void)reliable_msg; // Avoid compiler warning, depending on what transports are available
			}
		}
	}

	// Already closed?
	switch ( GetState() )
	{
		default:
		case k_EGameNetworkingConnectionState_None:
		case k_EGameNetworkingConnectionState_Dead: // shouldn't be in the map!
			Assert( false );
			// FALLTHROUGH
		case k_EGameNetworkingConnectionState_FinWait:
		case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
			SendConnectionClosedSignal( usecNow );
			return true;

		case k_EGameNetworkingConnectionState_ClosedByPeer:
			// Must be stray / out of order message, since we think they already closed
			// the connection.
			SendNoConnectionSignal( usecNow );
			return true;

		case k_EGameNetworkingConnectionState_Connecting:

			if ( msg.has_connect_ok() )
			{
				if ( m_bConnectionInitiatedRemotely )
				{
					SpewWarningGroup( LogLevel_P2PRendezvous(), "[%s] Ignoring P2P connect_ok, since they initiated the connection\n", GetDescription() );
					return false;
				}

				SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] Received ConnectOK in P2P Rendezvous.\n", GetDescription() );
				ProcessSignal_ConnectOK( msg.connect_ok(), usecNow );
			}
			break;

		case k_EGameNetworkingConnectionState_Linger:
		case k_EGameNetworkingConnectionState_FindingRoute:
		case k_EGameNetworkingConnectionState_Connected:

			// Now that we know we still might want to talk to them,
			// check for redundant connection request.  (Our reply dropped.)
			if ( msg.has_connect_request() )
			{
				if ( m_bConnectionInitiatedRemotely )
				{
					// NOTE: We're assuming here that it actually is a redundant retry,
					//       meaning they specified all the same parameters as before!
					SendConnectOKSignal( usecNow );
				}
				else
				{
					AssertMsg( false, "Received ConnectRequest in P2P rendezvous message, but we are the 'client'!" );
				}
			}
			break;
	}

	return true;
}

void CGameNetworkConnectionP2P::ProcessSignal_ConnectOK( const CMsgGameNetworkingP2PRendezvous_ConnectOK &msgConnectOK, GameNetworkingMicroseconds usecNow )
{
	Assert( !m_bConnectionInitiatedRemotely );

	// Check the certs, save keys, etc
	if ( !BRecvCryptoHandshake( msgConnectOK.cert(), msgConnectOK.crypt(), false ) )
	{
		Assert( GetState() == k_EGameNetworkingConnectionState_ProblemDetectedLocally );
		SpewWarning( "Failed crypto init in ConnectOK packet.  %s", m_szEndDebug );
		return;
	}

	// Mark that we received something.  Even though it was through the
	// signaling mechanism, not the channel used for data, and we ordinarily
	// don't count that.
	m_statsEndToEnd.m_usecTimeLastRecv = usecNow;

	// We're not fully connected.  Now we're doing rendezvous
	ConnectionState_FindingRoute( usecNow );
}

EGameNetConnectionEnd CGameNetworkConnectionP2P::CheckRemoteCert( const CertAuthScope *pCACertAuthScope, GameNetworkingErrMsg &errMsg )
{
	// Standard base class connection checks
	EGameNetConnectionEnd result = CGameNetworkConnectionBase::CheckRemoteCert( pCACertAuthScope, errMsg );
	if ( result != k_EGameNetConnectionEnd_Invalid )
		return result;

	// If ticket was bound to a data center, then make sure the cert chain authorizes
	// them to send us there.
	#ifdef SDR_ENABLE_HOSTED_CLIENT
		if ( m_pTransportToSDRServer )
		{
			GameNetworkingPOPID popIDTicket = m_pTransportToSDRServer->m_authTicket.m_ticket.m_routing.GetPopID();
			if ( popIDTicket != 0 && popIDTicket != k_SteamDatagramPOPID_dev )
			{
				if ( !CheckCertPOPID( m_msgCertRemote, pCACertAuthScope, popIDTicket, errMsg ) )
					return k_EGameNetConnectionEnd_Remote_BadCert;
			}
		}
	#endif

	return k_EGameNetConnectionEnd_Invalid;
}

void CGameNetworkConnectionP2P::QueueSignalReliableMessage( CMsgGameNetworkingP2PRendezvous_ReliableMessage &&msg, const char *pszDebug )
{
	AssertLocksHeldByCurrentThread();

	SpewVerboseGroup( LogLevel_P2PRendezvous(), "[%s] Queue reliable signal message %s: { %s }\n", GetDescription(), pszDebug, msg.ShortDebugString().c_str() );
	OutboundMessage *p = push_back_get_ptr( m_vecUnackedOutboundMessages );
	p->m_nID = ++m_nLastSendRendesvousMessageID;
	p->m_usecRTO = 1;
	p->m_msg = std::move( msg );
	p->m_cbSerialized = ProtoMsgByteSize(p->m_msg);
	ScheduleSendSignal( pszDebug );
}

void CGameNetworkConnectionP2P::ScheduleSendSignal( const char *pszReason )
{
	GameNetworkingMicroseconds usecDeadline = GameNetworkingSockets_GetLocalTimestamp() + 10*1000;
	if ( !m_pszNeedToSendSignalReason || m_usecSendSignalDeadline > usecDeadline )
	{
		m_pszNeedToSendSignalReason = pszReason;
		m_usecSendSignalDeadline = usecDeadline;
	}
	EnsureMinThinkTime( m_usecSendSignalDeadline );
}

void CGameNetworkConnectionP2P::PeerSelectedTransportChanged()
{
	AssertLocksHeldByCurrentThread();

	// If we are not the controlling agent, then we probably need to switch
	if ( !IsControllingAgent() && m_pPeerSelectedTransport != m_pCurrentTransportP2P )
	{
		m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
		m_bTransportSticky = false;
		SetNextThinkTimeASAP();
	}

	if ( m_pPeerSelectedTransport )
		SpewMsgGroup( LogLevel_P2PRendezvous(), "[%s] Peer appears to be using '%s' transport as primary\n", GetDescription(), m_pPeerSelectedTransport->m_pszP2PTransportDebugName );
}

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkingSockets CConnectionTransportP2PBase
//
/////////////////////////////////////////////////////////////////////////////

CConnectionTransportP2PBase::CConnectionTransportP2PBase( const char *pszDebugName, CConnectionTransport *pSelfBase )
: m_pszP2PTransportDebugName( pszDebugName )
, m_pSelfAsConnectionTransport( pSelfBase )
{
	m_pingEndToEnd.Reset();
	m_usecEndToEndInFlightReplyTimeout = 0;
	m_nReplyTimeoutsSinceLastRecv = 0;
	m_nKeepTryingToPingCounter = 5;
	m_usecWhenSelected = 0;
	m_usecTimeSelectedAccumulator = 0;
	m_bNeedToConfirmEndToEndConnectivity = true;
	m_routeMetrics.SetInvalid();
}

CConnectionTransportP2PBase::~CConnectionTransportP2PBase()
{
	CGameNetworkConnectionP2P &conn = Connection();
	conn.AssertLocksHeldByCurrentThread();

	// Detach from parent connection
	find_and_remove_element( conn.m_vecAvailableTransports, this );

	Assert( ( conn.m_pTransport == m_pSelfAsConnectionTransport ) == ( conn.m_pCurrentTransportP2P == this ) );
	if ( conn.m_pTransport == m_pSelfAsConnectionTransport || conn.m_pCurrentTransportP2P == this )
		conn.SelectTransport( nullptr, GameNetworkingSockets_GetLocalTimestamp() );
	if ( conn.m_pPeerSelectedTransport == this )
		conn.m_pPeerSelectedTransport = nullptr;

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		if ( conn.m_pTransportP2PSDR == this )
			conn.m_pTransportP2PSDR = nullptr;
	#endif

	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
		if ( conn.m_pTransportICE == this )
			conn.m_pTransportICE = nullptr;
		if ( conn.m_pTransportICEPendingDelete == this )
			conn.m_pTransportICEPendingDelete = nullptr;
	#endif

	// Make sure we re-evaluate transport
	conn.m_usecNextEvaluateTransport = k_nThinkTime_ASAP;
	conn.SetNextThinkTimeASAP();
}

void CConnectionTransportP2PBase::P2PTransportTrackSentEndToEndPingRequest( GameNetworkingMicroseconds usecNow, bool bAllowDelayedReply )
{
	m_pingEndToEnd.m_usecTimeLastSentPingRequest = usecNow;
	if ( m_usecEndToEndInFlightReplyTimeout == 0 )
	{
		if ( m_nKeepTryingToPingCounter > 0 )
			--m_nKeepTryingToPingCounter;
		m_usecEndToEndInFlightReplyTimeout = usecNow + m_pingEndToEnd.CalcConservativeTimeout();
		if ( bAllowDelayedReply )
			m_usecEndToEndInFlightReplyTimeout += k_usecSteamDatagramRouterPendClientPing; // Is this the appropriate constant to use?

		EnsureP2PTransportThink( m_usecEndToEndInFlightReplyTimeout );
	}
}

void CConnectionTransportP2PBase::P2PTransportThink( GameNetworkingMicroseconds usecNow )
{
	CGameNetworkConnectionP2P &conn = Connection();
	conn.AssertLocksHeldByCurrentThread( "P2PTransportThink" );

	// We only need to take action while connecting, or trying to connect
	switch ( conn.GetState() )
	{

		case k_EGameNetworkingConnectionState_FindingRoute:
		case k_EGameNetworkingConnectionState_Connected:
		case k_EGameNetworkingConnectionState_Linger:
			break;

		default:

			// We'll have to wait until we get a callback
			return;
	}

	// Check for reply timeout
	if ( m_usecEndToEndInFlightReplyTimeout )
	{
		if ( m_usecEndToEndInFlightReplyTimeout < usecNow )
		{
			m_usecEndToEndInFlightReplyTimeout = 0;
			++m_nReplyTimeoutsSinceLastRecv;
			if ( m_nReplyTimeoutsSinceLastRecv > 2 && !m_bNeedToConfirmEndToEndConnectivity )
			{
				SpewMsg( "[%s] %s: %d consecutive end-to-end timeouts\n",
					conn.GetDescription(), m_pszP2PTransportDebugName, m_nReplyTimeoutsSinceLastRecv );
				P2PTransportEndToEndConnectivityNotConfirmed( usecNow );
				conn.TransportEndToEndConnectivityChanged( this, usecNow );
			}
		}
	}

	// Check back in periodically
	GameNetworkingMicroseconds usecNextThink = usecNow + 2*k_nMillion;

	// Check for sending ping requests
	if ( m_usecEndToEndInFlightReplyTimeout == 0 && m_pSelfAsConnectionTransport->BCanSendEndToEndData() )
	{

		// Check for pinging as fast as possible until we get an initial ping sample.
		CConnectionTransportP2PBase *pCurrentP2PTransport = Connection().m_pCurrentTransportP2P;
		if ( m_nKeepTryingToPingCounter > 0 )
		{
			m_pSelfAsConnectionTransport->SendEndToEndStatsMsg( k_EStatsReplyRequest_Immediate, usecNow, "End-to-end ping sample" );
		}
		else if ( 
			pCurrentP2PTransport == this // they have selected us
			|| pCurrentP2PTransport == nullptr // They haven't selected anybody
			|| pCurrentP2PTransport->m_bNeedToConfirmEndToEndConnectivity // current transport is not in good shape
		) {

			// We're a viable option right now, not just a backup
			if ( 
				// Some reason to establish connectivity or collect more data?
				m_bNeedToConfirmEndToEndConnectivity
				|| m_nReplyTimeoutsSinceLastRecv > 0
				|| m_pingEndToEnd.m_nSmoothedPing < 0
				|| m_pingEndToEnd.m_nValidPings < V_ARRAYSIZE(m_pingEndToEnd.m_arPing)
				|| m_pingEndToEnd.m_nTotalPingsReceived < 10
			) {
				m_pSelfAsConnectionTransport->SendEndToEndStatsMsg( k_EStatsReplyRequest_Immediate, usecNow, "Connectivity check" );
			}
			else
			{
				// We're the current transport and everything looks good.  We will let
				// the end-to-end keepalives handle things, no need to take our own action here.
			}
		}
		else
		{
			// They are using some other transport.  Just ping every now and then
			// so that if conditions change, we could discover that we are better
			GameNetworkingMicroseconds usecNextPing = m_pingEndToEnd.m_usecTimeLastSentPingRequest + 10*k_nMillion;
			if ( usecNextPing <= usecNow )
			{
				m_pSelfAsConnectionTransport->SendEndToEndStatsMsg( k_EStatsReplyRequest_DelayedOK, usecNow, "P2PGrassGreenerCheck" );
			}
			else
			{
				usecNextThink = std::min( usecNextThink, usecNextPing );
			}
		}
	}

	if ( m_usecEndToEndInFlightReplyTimeout )
		usecNextThink = std::min( usecNextThink, m_usecEndToEndInFlightReplyTimeout );
	EnsureP2PTransportThink( usecNextThink );
}

void CConnectionTransportP2PBase::P2PTransportEndToEndConnectivityNotConfirmed( GameNetworkingMicroseconds usecNow )
{
	if ( !m_bNeedToConfirmEndToEndConnectivity )
		return;
	CGameNetworkConnectionP2P &conn = Connection();
	SpewWarningGroup( conn.LogLevel_P2PRendezvous(), "[%s] %s end-to-end connectivity lost\n", conn.GetDescription(), m_pszP2PTransportDebugName );
	m_bNeedToConfirmEndToEndConnectivity = true;
	conn.TransportEndToEndConnectivityChanged( this, usecNow );
}

void CConnectionTransportP2PBase::P2PTransportEndToEndConnectivityConfirmed( GameNetworkingMicroseconds usecNow )
{
	CGameNetworkConnectionP2P &conn = Connection();

	if ( !m_pSelfAsConnectionTransport->BCanSendEndToEndData() )
	{
		AssertMsg2( false, "[%s] %s trying to mark connectivity as confirmed, but !BCanSendEndToEndData!", conn.GetDescription(), m_pszP2PTransportDebugName );
		return;
	}

	if ( m_bNeedToConfirmEndToEndConnectivity )
	{
		SpewVerboseGroup( conn.LogLevel_P2PRendezvous(), "[%s] %s end-to-end connectivity confirmed\n", conn.GetDescription(), m_pszP2PTransportDebugName );
		m_bNeedToConfirmEndToEndConnectivity = false;
		conn.TransportEndToEndConnectivityChanged( this, usecNow );
	}
}

GameNetworkingMicroseconds CConnectionTransportP2PBase::CalcTotalTimeSelected( GameNetworkingMicroseconds usecNow ) const
{
	GameNetworkingMicroseconds result = m_usecTimeSelectedAccumulator;
	if ( m_usecWhenSelected > 0 )
	{
		GameNetworkingMicroseconds whenEnded = Connection().m_statsEndToEnd.m_usecWhenEndedConnectedState;
		if ( whenEnded == 0 )
			whenEnded = usecNow;
		Assert( whenEnded >= m_usecWhenSelected );
		result += usecNow - m_usecWhenSelected;
	}
	return result;
}

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkingSockets P2P stuff
//
/////////////////////////////////////////////////////////////////////////////

HSteamListenSocket CGameNetworkingSockets::CreateListenSocketP2P( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	// Despite the API argument being an int, we'd like to reserve most of the address space.
	if ( nLocalVirtualPort < 0 || nLocalVirtualPort > 0xffff )
	{
		SpewError( "Virtual port number must be a small, positive number" );
		return k_HSteamListenSocket_Invalid;
	}

	GameNetworkingGlobalLock scopeLock( "CreateListenSocketP2P" );

	CGameNetworkListenSocketP2P *pSock = InternalCreateListenSocketP2P( nLocalVirtualPort, nOptions, pOptions );
	if ( pSock )
		return pSock->m_hListenSocketSelf;
	return k_HSteamListenSocket_Invalid;
}

CGameNetworkListenSocketP2P *CGameNetworkingSockets::InternalCreateListenSocketP2P( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "InternalCreateListenSocketP2P" );

	SteamDatagramErrMsg errMsg;

	// We'll need a cert.  Start sure async process to get one is in
	// progress (or try again if we tried earlier and failed)
	AuthenticationNeeded();

	// Figure out what kind of socket to create.
	// Hosted dedicated server?
	CGameNetworkListenSocketP2P *pSock = nullptr;
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
		CGameNetworkingSocketsSDR *pGameNetworkingSocketsSDR = assert_cast< CGameNetworkingSocketsSDR *>( this );

		#ifdef SDR_ENABLE_HOSTED_SERVER
			if ( pGameNetworkingSocketsSDR->GetHostedDedicatedServerPort() != 0 )
			{
				if ( !pGameNetworkingSocketsSDR->m_bGameServer )
				{
					// It's totally possible that this works fine.  But it's weird and untested, and
					// almost certainly a bug somewhere, so let's just disallow it until we know what
					// the use case is.
					AssertMsg( false, "Can't create a P2P listen socket on a 'user' interface in a hosted dedicated server" );
					return nullptr;
				}
				pSock = new CGameNetworkListenSocketSDRServer( pGameNetworkingSocketsSDR );
			}
		#endif

		if ( !pSock )
		{
			// We're not in a hosted dedicated server, so it's the usual P2P stuff.
			if ( !pGameNetworkingSocketsSDR->BSDRClientInit( errMsg ) )
				return nullptr;
		}
	#endif

	// Ordinary case where we are not at known data center?
	if ( !pSock )
	{
		pSock = new CGameNetworkListenSocketP2P( this );
		if ( !pSock )
			return nullptr;
	}

	// Create listen socket
	if ( !pSock->BInit( nLocalVirtualPort, nOptions, pOptions, errMsg ) )
	{
		SpewError( "Cannot create listen socket.  %s", errMsg );
		pSock->Destroy();
		return nullptr;
	}

	return pSock;
}

HGameNetConnection CGameNetworkingSockets::ConnectP2P( const GameNetworkingIdentity &identityRemote, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	// Despite the API argument being an int, we'd like to reserve most of the address space.

	if ( nRemoteVirtualPort < 0 || nRemoteVirtualPort > 0xffff )
	{
		SpewError( "Virtual port number should be a small, non-negative number\n" );
		return k_HGameNetConnection_Invalid;
	}

	GameNetworkingGlobalLock scopeLock( "ConnectP2P" );
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = InternalConnectP2PDefaultSignaling( identityRemote, nRemoteVirtualPort, nOptions, pOptions, connectionLock );
	if ( pConn )
		return pConn->m_hConnectionSelf;
	return k_HGameNetConnection_Invalid;
}

CGameNetworkConnectionBase *CGameNetworkingSockets::InternalConnectP2PDefaultSignaling(
	const GameNetworkingIdentity &identityRemote,
	int nRemoteVirtualPort,
	int nOptions, const GameNetworkingConfigValue_t *pOptions,
	ConnectionScopeLock &scopeLock
)
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "InternalConnectP2PDefaultSignaling" );
	if ( identityRemote.IsInvalid() )
	{
		AssertMsg( false, "Invalid identity" );
		return nullptr;
	}

	SteamDatagramErrMsg errMsg;

	// Check for connecting to an identity in this process
	for ( CGameNetworkingSockets *pLocalInstance: CGameNetworkingSockets::s_vecGameNetworkingSocketsInstances )
	{
		if ( pLocalInstance->InternalGetIdentity() == identityRemote )
		{

			// This is the guy we want to talk to.  Are we listening on that virtual port?
			int idx = pLocalInstance->m_mapListenSocketsByVirtualPort.Find( nRemoteVirtualPort );
			if ( idx == pLocalInstance->m_mapListenSocketsByVirtualPort.InvalidIndex() )
			{
				SpewError( "Cannot create P2P connection to local identity %s.  We are not listening on vport %d", GameNetworkingIdentityRender( identityRemote ).c_str(), nRemoteVirtualPort );
				return nullptr;
			}

			// Create a loopback connection
			CGameNetworkConnectionPipe *pConn = CGameNetworkConnectionPipe::CreateLoopbackConnection( this, nOptions, pOptions, pLocalInstance->m_mapListenSocketsByVirtualPort[ idx ], errMsg, scopeLock );
			if ( pConn )
			{
				SpewVerbose( "[%s] Using loopback for P2P connection to local identity %s on vport %d.  Partner is [%s]\n",
					pConn->GetDescription(),
					GameNetworkingIdentityRender( identityRemote ).c_str(), nRemoteVirtualPort,
					pConn->m_pPartner->GetDescription() );
				return pConn;
			}

			// Failed?
			SpewError( "P2P connection to local identity %s on vport %d; FAILED to create loopback.  %s\n",
				GameNetworkingIdentityRender( identityRemote ).c_str(), nRemoteVirtualPort, errMsg );
			return nullptr;
		}
	}

	// What local virtual port will be used?
	int nLocalVirtualPort = nRemoteVirtualPort;
	for ( int idxOpt = 0 ; idxOpt < nOptions ; ++idxOpt )
	{
		if ( pOptions[idxOpt].m_eValue == k_EGameNetworkingConfig_LocalVirtualPort )
		{
			if ( pOptions[idxOpt].m_eDataType == k_EGameNetworkingConfig_Int32 )
			{
				nLocalVirtualPort = pOptions[idxOpt].m_val.m_int32;
			}
			else
			{
				SpewBug( "LocalVirtualPort must be Int32" );
				return nullptr;
			}
		}
	}

	// Create signaling
	FnGameNetworkingSocketsCreateConnectionSignaling fnCreateConnectionSignaling = (FnGameNetworkingSocketsCreateConnectionSignaling)g_Config_Callback_CreateConnectionSignaling.Get();
	if ( fnCreateConnectionSignaling == nullptr )
	{
		SpewBug( "Cannot use P2P connectivity.  CreateConnectionSignaling callback not set" );
		return nullptr;
	}
	IGameNetworkingConnectionSignaling *pSignaling = (*fnCreateConnectionSignaling)( this, identityRemote, nLocalVirtualPort, nRemoteVirtualPort );
	if ( !pSignaling )
		return nullptr;

	// Use the generic path
	CGameNetworkConnectionBase *pResult = InternalConnectP2P( pSignaling, &identityRemote, nRemoteVirtualPort, nOptions, pOptions, scopeLock );

	// Confirm that we properly knew what the local virtual port would be
	Assert( !pResult || pResult->m_connectionConfig.m_LocalVirtualPort.Get() == nLocalVirtualPort );

	// Done
	return pResult;
}

HGameNetConnection CGameNetworkingSockets::ConnectP2PCustomSignaling( IGameNetworkingConnectionSignaling *pSignaling, const GameNetworkingIdentity *pPeerIdentity, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	if ( !pSignaling )
		return k_HGameNetConnection_Invalid;

	GameNetworkingGlobalLock scopeLock( "ConnectP2PCustomSignaling" );
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = InternalConnectP2P( pSignaling, pPeerIdentity, nRemoteVirtualPort, nOptions, pOptions, connectionLock );
	if ( pConn )
		return pConn->m_hConnectionSelf;
	return k_HGameNetConnection_Invalid;
}

CGameNetworkConnectionBase *CGameNetworkingSockets::InternalConnectP2P(
	IGameNetworkingConnectionSignaling *pSignaling,
	const GameNetworkingIdentity *pPeerIdentity,
	int nRemoteVirtualPort,
	int nOptions, const GameNetworkingConfigValue_t *pOptions,
	ConnectionScopeLock &scopeLock
)
{
	CGameNetworkConnectionP2P *pConn = new CGameNetworkConnectionP2P( this, scopeLock );
	if ( !pConn )
	{
		pSignaling->Release();
		return nullptr;
	}

	SteamDatagramErrMsg errMsg;
	CGameNetworkConnectionP2P *pMatchingConnection = nullptr;
	if ( pConn->BInitConnect( pSignaling, pPeerIdentity, nRemoteVirtualPort, nOptions, pOptions, &pMatchingConnection, errMsg ) )
		return pConn;

	// Failed.  Destroy the failed connection
	pConn->ConnectionQueueDestroy();
	scopeLock.Unlock();
	pConn = nullptr;

	// Did we fail because we found an existing matching connection?
	if ( pMatchingConnection )
	{
		scopeLock.Lock( *pMatchingConnection, "InternalConnectP2P Matching Accept" );

		// If connection is inbound, then we can just implicitly accept it.
		if ( !pMatchingConnection->m_bConnectionInitiatedRemotely || pMatchingConnection->GetState() != k_EGameNetworkingConnectionState_Connecting )
		{
			V_sprintf_safe( errMsg, "Found existing connection [%s].  Only one symmetric connection can be active at a time.", pMatchingConnection->GetDescription() );
		}
		else
		{
			SpewVerbose( "[%s] Accepting inbound connection implicitly, based on matching outbound connect request\n", pMatchingConnection->GetDescription() );

			// OK, we can try to accept this connection.  HOWEVER, first let's apply
			// any connection options the caller is passing in.
			if ( pOptions )
			{
				for ( int i = 0 ; i < nOptions ; ++i )
				{
					const GameNetworkingConfigValue_t &opt = pOptions[i];

					// Skip these, they are locked
					if ( opt.m_eValue == k_EGameNetworkingConfig_LocalVirtualPort || opt.m_eValue == k_EGameNetworkingConfig_SymmetricConnect )
						continue;

					// Set the option
					if ( !m_pGameNetworkingUtils->SetConfigValueStruct( opt, k_EGameNetworkingConfig_Connection, pMatchingConnection->m_hConnectionSelf ) )
					{
						// Spew, but keep going!
						SpewBug( errMsg, "[%s] Failed to set option %d while implicitly accepting.  Ignoring failure!", pMatchingConnection->GetDescription(), opt.m_eValue );
					}
				}
			}
			else
			{
				Assert( nOptions == 0 );
			}

			// Implicitly accept connection
			EResult eAcceptResult = pMatchingConnection->AcceptConnection( GameNetworkingSockets_GetLocalTimestamp() );
			if ( eAcceptResult != k_EResultOK )
			{
				SpewBug( errMsg, "[%s] Failed to implicitly accept with return code %d", pMatchingConnection->GetDescription(), eAcceptResult );
				return nullptr;
			}

			// All good!  Return the incoming connection that was accepted
			return pMatchingConnection;
		}
	}

	// Failed
	if ( pPeerIdentity )
		SpewError( "Cannot create P2P connection to %s.  %s", GameNetworkingIdentityRender( *pPeerIdentity ).c_str(), errMsg );
	else
		SpewError( "Cannot create P2P connection.  %s", errMsg );
	return nullptr;
}

static void SendP2PRejection( IGameNetworkingSignalingRecvContext *pContext, GameNetworkingIdentity &identityPeer, const CMsgGameNetworkingP2PRendezvous &msg, int nEndReason, const char *fmt, ... )
{
	if ( !msg.from_connection_id() || msg.from_identity().empty() )
		return;

	char szDebug[ 256 ];
	va_list ap;
	va_start( ap, fmt );
	V_vsnprintf( szDebug, sizeof(szDebug), fmt, ap );
	va_end( ap );

	CMsgGameNetworkingP2PRendezvous msgReply;
	msgReply.set_to_connection_id( msg.from_connection_id() );
	msgReply.set_to_identity( msg.from_identity() );
	msgReply.mutable_connection_closed()->set_reason_code( nEndReason );
	msgReply.mutable_connection_closed()->set_debug( szDebug );

	int cbReply = ProtoMsgByteSize( msgReply );
	uint8 *pReply = (uint8*)alloca( cbReply );
	DbgVerify( msgReply.SerializeWithCachedSizesToArray( pReply ) == pReply + cbReply );
	pContext->SendRejectionSignal( identityPeer, pReply, cbReply );
}

bool CGameNetworkingSockets::ReceivedP2PCustomSignal( const void *pMsg, int cbMsg, IGameNetworkingSignalingRecvContext *pContext )
{
	return InternalReceivedP2PSignal( pMsg, cbMsg, pContext, false );
}

// Compare connections initiated by two peers, and make a decision
// which one should take priority.  We use the Connection IDs as the
// first discriminator, and we do it in a "rock-paper-scissors" sort
// of a way, such that all IDs are equally likely to win if you don't
// know the other ID, and a malicious client has no strategy for
// influencing the outcome to achieve any particular end.
//
// <0: A should be the "client"
// >0: B should be the "client"
// 0: cannot choose.  (*exceedingly* rare)
static int CompareSymmetricConnections( uint32 nConnectionIDA, const std::string &sTieBreakerA, uint32 nConnectionIDB, const std::string &sTieBreakerB )
{

	// Start by select
	int result;
	if ( nConnectionIDA < nConnectionIDB ) result = -1;
	else if ( nConnectionIDA > nConnectionIDB ) result = +1;
	else
	{
		// This is exceedingly rare.  We go ahead and write some code to handle it, because
		// why not?  It would probably be acceptable to punt here and fail the connection.
		// But assert, because if we do hit this case, it is almost certainly more likely
		// to be a bug in our code than an actual collision.
		//
		// Also note that it is possible to make a connection to "yourself"
		AssertMsg( false, "Symmetric connections with connection IDs!  Odds are 1:2e32!" );

		// Compare a secondary source of entropy.  Even if encryption is disabled, we still create a key per connection.
		int n = std::min( len( sTieBreakerA ), len( sTieBreakerB ) );
		Assert( n >= 32 );
		result = memcmp( sTieBreakerA.c_str(), sTieBreakerB.c_str(), n );
		Assert( result != 0 );
	}

	// Check parity of lowest bit and flip result
	if ( ( nConnectionIDA ^ nConnectionIDB ) & 1 )
		result = -result;

	return result;
}

bool CGameNetworkingSockets::InternalReceivedP2PSignal( const void *pMsg, int cbMsg, IGameNetworkingSignalingRecvContext *pContext, bool bDefaultSignaling )
{
	SteamDatagramErrMsg errMsg;

	// Deserialize the message
	CMsgGameNetworkingP2PRendezvous msg;
	if ( !msg.ParseFromArray( pMsg, cbMsg ) )
	{
		SpewWarning( "P2P signal failed protobuf parse\n" );
		return false;
	}

	// Parse remote identity
	if ( *msg.from_identity().c_str() == '\0' )
	{
		SpewWarning( "Bad P2P signal: no from_identity\n" );
		return false;
	}
	GameNetworkingIdentity identityRemote;
	if ( !identityRemote.ParseString( msg.from_identity().c_str() ) )
	{
		SpewWarning( "Bad P2P signal: invalid from_identity '%s'\n", msg.from_identity().c_str() );
		return false;
	}

	int nLogLevel = m_connectionConfig.m_LogLevel_P2PRendezvous.Get();

	// Grab the lock now.  (We might not have previously held it.)
	GameNetworkingGlobalLock lock( "ReceivedP2PSignal" );

	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	// Locate the connection, if we already have one
	CGameNetworkConnectionP2P *pConn = nullptr;
	ConnectionScopeLock connectionLock;
	if ( msg.has_to_connection_id() )
	{
		CGameNetworkConnectionBase *pConnBase = FindConnectionByLocalID( msg.to_connection_id(), connectionLock );

		// Didn't find them?  Then just drop it.  Otherwise, we are susceptible to leaking the player's online state
		// any time we receive random message.
		if ( pConnBase == nullptr )
		{
			SpewMsgGroup( nLogLevel, "Ignoring P2PRendezvous from %s to unknown connection #%u\n", GameNetworkingIdentityRender( identityRemote ).c_str(), msg.to_connection_id() );
			return true;
		}

		SpewVerboseGroup( nLogLevel, "[%s] Recv P2PRendezvous\n", pConnBase->GetDescription() );
		SpewDebugGroup( nLogLevel, "%s\n\n", Indent( msg.DebugString() ).c_str() );

		pConn = pConnBase->AsGameNetworkConnectionP2P();
		if ( !pConn )
		{
			SpewWarning( "[%s] Got P2P signal from %s.  Wrong connection type!\n", msg.from_identity().c_str(), pConn->GetDescription() );
			return false;
		}

		// Connection already shutdown?
		if ( pConn->GetState() == k_EGameNetworkingConnectionState_Dead )
		{
			// How was the connection found by FindConnectionByLocalID then?
			Assert( false );
			return false;
		}

		// We might not know who the other guy is yet
		if ( pConn->GetState() == k_EGameNetworkingConnectionState_Connecting && ( pConn->m_identityRemote.IsInvalid() || pConn->m_identityRemote.IsLocalHost() ) )
		{
			pConn->m_identityRemote = identityRemote;
			pConn->SetDescription();
		}
		else if ( !( pConn->m_identityRemote == identityRemote ) )
		{
			SpewWarning( "[%s] Got P2P signal from wrong remote identity '%s'\n", pConn->GetDescription(), msg.from_identity().c_str() );
			return false;
		}

		// They should always send their connection ID, unless they never established a connection.
		if ( pConn->m_unConnectionIDRemote )
		{
			if ( pConn->m_unConnectionIDRemote != msg.from_connection_id() )
			{
				SpewWarning( "Ignoring P2P signal from %s.  For our cxn #%u, they first used remote cxn #%u, not using #%u",
					msg.from_identity().c_str(), msg.to_connection_id(), pConn->m_unConnectionIDRemote, msg.from_connection_id() );
				return false;
			}
		}
		else
		{
			pConn->m_unConnectionIDRemote = msg.from_connection_id();
		}
		if ( !pConn->BEnsureInP2PConnectionMapByRemoteInfo( errMsg ) )
			return false;
	}
	else
	{

		// They didn't know our connection ID (yet).  But we might recognize their connection ID.
		if ( !msg.from_connection_id() )
		{
			SpewWarning( "Bad P2P signal from '%s': neither from/to connection IDs present\n", msg.from_identity().c_str() );
			return false;
		}
		RemoteConnectionKey_t key{ identityRemote, msg.from_connection_id() };
		int idxMapP2P = g_mapP2PConnectionsByRemoteInfo.Find( key );
		if ( idxMapP2P != g_mapP2PConnectionsByRemoteInfo.InvalidIndex() )
		{
			pConn = g_mapP2PConnectionsByRemoteInfo[ idxMapP2P ];
			connectionLock.Lock( *pConn );
			Assert( pConn->m_idxMapP2PConnectionsByRemoteInfo == idxMapP2P );
			Assert( pConn->m_identityRemote == identityRemote );
			Assert( pConn->m_unConnectionIDRemote == msg.from_connection_id() );
		}
		else
		{

			// Only other legit case is a new connect request.
			if ( !msg.has_connect_request() )
			{
				SpewWarning( "Ignoring P2P signal from '%s', unknown remote connection #%u\n", msg.from_identity().c_str(), msg.from_connection_id() );

				// We unfortunately must not reply in this case.  If we do reply,
				// then all you need to do to tell if somebody is online is send a
				// signal with a random connection ID.  If we did have such a
				// connection, but it is deleted now, then hopefully we cleaned it
				// up properly, handling potential for dropped cleanup messages,
				// in the FinWait state
				return true;
			}

			// We must know who we are.
			if ( m_identity.IsInvalid() )
			{
				SpewWarning( "Ignoring P2P signal from '%s', no local identity\n", msg.from_identity().c_str() );
				return false;
			}

			// Are we ready with authentication?
			// This is actually not really correct to use a #define here.  Really, we ought
			// to create a connection and check AllowLocalUnsignedCert/AllowRemoteUnsignedCert.
			#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE

				// Make sure we have a recent cert.  Start requesting another if needed.
				AuthenticationNeeded();

				// If we don't have a signed cert now, then we cannot accept this connection!
				// P2P connections always require certs issued by Steam!
				if ( !m_msgSignedCert.has_ca_signature() )
				{
					SpewWarning( "Ignoring P2P connection request from %s.  We cannot accept it since we don't have a cert yet!\n",
						GameNetworkingIdentityRender( identityRemote ).c_str() );
					return true; // Return true because the signal is valid, we just cannot do anything with it right now
				}
			#endif

			const CMsgGameNetworkingP2PRendezvous_ConnectRequest &msgConnectRequest = msg.connect_request();
			if ( !msgConnectRequest.has_cert() || !msgConnectRequest.has_crypt() )
			{
				AssertMsg1( false, "Ignoring P2P CMsgSteamDatagramConnectRequest from %s; missing required fields", GameNetworkingIdentityRender( identityRemote ).c_str() );
				return false;
			}

			// Determine virtual ports, and locate the listen socket, if any
			int nLocalVirtualPort = -1;
			int nRemoteVirtualPort = -1;
			bool bSymmetricListenSocket = false;
			CGameNetworkListenSocketP2P *pListenSock = nullptr;
			if ( msgConnectRequest.has_to_virtual_port() )
			{
				nLocalVirtualPort = msgConnectRequest.to_virtual_port();
				if ( msgConnectRequest.has_from_virtual_port() )
					nRemoteVirtualPort = msgConnectRequest.from_virtual_port();
				else
					nRemoteVirtualPort = nLocalVirtualPort;

				// Connection for IGameNetworkingMessages system
				if ( nLocalVirtualPort == k_nVirtualPort_Messages )
				{
					#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES

						// Make sure messages system is initialized
						if ( !GetGameNetworkingMessages() )
						{
							SpewBug( "Ignoring P2P CMsgSteamDatagramConnectRequest from %s; can't get IGameNetworkingNetworkingMessages interface!", GameNetworkingIdentityRender( identityRemote ).c_str() );
							//SendP2PRejection( pContext, identityRemote, msg, k_EGameNetConnectionEnd_Misc_Generic, "Internal error accepting connection.  Can't get NetworkingMessages interface" );
							return false;
						}
					#else
						SpewWarning( "Ignoring P2P CMsgSteamDatagramConnectRequest from %s; IGameNetworkingNetworkingMessages not supported", GameNetworkingIdentityRender( identityRemote ).c_str() );
						//SendP2PRejection( pContext, identityRemote, msg, k_EGameNetConnectionEnd_Misc_Generic, "Internal error accepting connection.  Can't get NetworkingMessages interface" );
						return false;
					#endif
				}

				// Locate the listen socket
				int idxListenSock = m_mapListenSocketsByVirtualPort.Find( nLocalVirtualPort );
				if ( idxListenSock == m_mapListenSocketsByVirtualPort.InvalidIndex() )
				{

					// If default signaling, then it must match up to a listen socket.
					// If custom signaling, then they need not have created one.
					if ( bDefaultSignaling )
					{

						// Totally ignore it.  We don't want this to be able to be used as a way to
						// tell if you are online or not.
						SpewMsgGroup( nLogLevel, "Ignoring P2P CMsgSteamDatagramConnectRequest from %s; we're not listening on vport %d\n", GameNetworkingIdentityRender( identityRemote ).c_str(), nLocalVirtualPort );
						return false;
					}
				}
				else
				{
					pListenSock = m_mapListenSocketsByVirtualPort[ idxListenSock ];
					bSymmetricListenSocket = pListenSock->BSymmetricMode();
				}

				// Check for matching symmetric connections
				if ( nLocalVirtualPort >= 0 )
				{
					bool bOnlySymmetricConnections = !bSymmetricListenSocket; // If listen socket is symmetric, than any other existing connection counts.  Otherwise, we only conflict with existing connections opened in symmetric mode
					CGameNetworkConnectionP2P *pMatchingConnection = CGameNetworkConnectionP2P::FindDuplicateConnection( this, nLocalVirtualPort, identityRemote, nRemoteVirtualPort, bOnlySymmetricConnections, nullptr );
					if ( pMatchingConnection )
					{
						ConnectionScopeLock lockMatchingConnection( *pMatchingConnection );
						Assert( pMatchingConnection->m_pParentListenSocket == nullptr ); // This conflict should only happen for connections we initiate!
						int cmp = CompareSymmetricConnections( pMatchingConnection->m_unConnectionIDLocal, pMatchingConnection->GetSignedCertLocal().cert(), msg.from_connection_id(), msgConnectRequest.cert().cert() );

						// Check if we prefer for our connection to act as the "client"
						if ( cmp <= 0 )
						{
							SpewVerboseGroup( nLogLevel, "[%s] Symmetric role resolution for connect request remote cxn ID #%u says we should act as client.  Dropping incoming request, we will wait for them to accept ours\n", pMatchingConnection->GetDescription(), msg.from_connection_id() );
							Assert( !pMatchingConnection->m_bConnectionInitiatedRemotely );
							return true;
						}

						pMatchingConnection->ChangeRoleToServerAndAccept( msg, usecNow );
						return true;
					}
				}

			}
			else
			{
				// Old client using custom signaling that previously did not specify virtual ports.
				// This is OK
				Assert( !bDefaultSignaling );
			}

			// Special case for servers in known POPs
			#ifdef SDR_ENABLE_HOSTED_SERVER
				if ( pListenSock )
				{
					switch ( pListenSock->m_eHostedDedicatedServer )
					{
						case CGameNetworkListenSocketP2P::k_EHostedDedicatedServer_Not:
							// Normal P2P connectivity
							break;

						case CGameNetworkListenSocketP2P::k_EHostedDedicatedServer_TicketsOnly:
							SpewMsgGroup( nLogLevel, "Ignoring P2P CMsgSteamDatagramConnectRequest from %s; we're listening on vport %d, but only for ticket-based connections, not for connections requiring P2P signaling\n", GameNetworkingIdentityRender( identityRemote ).c_str(), nLocalVirtualPort );
							return false;

						case CGameNetworkListenSocketP2P::k_EHostedDedicatedServer_Auto:
							SpewMsgGroup( nLogLevel, "P2P CMsgSteamDatagramConnectRequest from %s; we're listening on vport %d, hosted server connection\n", GameNetworkingIdentityRender( identityRemote ).c_str(), nLocalVirtualPort );
							pConn = new CGameNetworkAcceptedConnectionFromSDRClient( this, connectionLock );
							break;

						default:
							Assert( false );
							return false;
					}
				}
			#endif

			// Create a connection
			if ( pConn == nullptr )
				pConn = new CGameNetworkConnectionP2P( this, connectionLock );
			pConn->m_identityRemote = identityRemote;
			pConn->m_unConnectionIDRemote = msg.from_connection_id();
			pConn->m_nRemoteVirtualPort = nRemoteVirtualPort;
			pConn->m_connectionConfig.m_LocalVirtualPort.Set( nLocalVirtualPort );
			if ( bSymmetricListenSocket )
			{
				pConn->m_connectionConfig.m_SymmetricConnect.Set( 1 );
				pConn->m_connectionConfig.m_SymmetricConnect.Lock();
			}

			// Suppress state change notifications for now
			Assert( pConn->m_nSupressStateChangeCallbacks == 0 );
			pConn->m_nSupressStateChangeCallbacks = 1;

			// Add it to the listen socket, if any
			if ( pListenSock )
			{
				if ( !pListenSock->BAddChildConnection( pConn, errMsg ) )
				{
					SpewWarning( "Failed to start accepting P2P connect request from %s on vport %d; %s\n",
						GameNetworkingIdentityRender( pConn->m_identityRemote ).c_str(), nLocalVirtualPort, errMsg );
					pConn->ConnectionQueueDestroy();
					return false;
				}
			}

			// OK, start setting up the connection
			if ( !pConn->BBeginAcceptFromSignal( 
				msgConnectRequest,
				errMsg,
				usecNow
			) ) {
				SpewWarning( "Failed to start accepting P2P connect request from %s on vport %d; %s\n",
					GameNetworkingIdentityRender( pConn->m_identityRemote ).c_str(), nLocalVirtualPort, errMsg );
				pConn->ConnectionQueueDestroy();
				SendP2PRejection( pContext, identityRemote, msg, k_EGameNetConnectionEnd_Misc_Generic, "Internal error accepting connection.  %s", errMsg );
				return false;
			}

			// Mark that we received something.  Even though it was through the
			// signaling mechanism, not the channel used for data, and we ordinarily
			// don't count that.
			pConn->m_statsEndToEnd.m_usecTimeLastRecv = usecNow;

			// Inform app about the incoming request, see what they want to do
			pConn->m_pSignaling = pContext->OnConnectRequest( pConn->m_hConnectionSelf, identityRemote, nLocalVirtualPort );

			// Already closed?
			switch ( pConn->GetState() )
			{
				default:
				case k_EGameNetworkingConnectionState_ClosedByPeer: // Uhhhhhh
				case k_EGameNetworkingConnectionState_Dead:
				case k_EGameNetworkingConnectionState_Linger:
				case k_EGameNetworkingConnectionState_None:
				case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
					Assert( false );
					// FALLTHROUGH
				case k_EGameNetworkingConnectionState_FinWait:

					// app context closed the connection.  This means that they want to send
					// a rejection
					SpewVerboseGroup( nLogLevel, "[%s] P2P connect request actively rejected by app, sending rejection (%s)\n",
						pConn->GetDescription(), pConn->GetConnectionEndDebugString() );
					SendP2PRejection( pContext, identityRemote, msg, pConn->GetConnectionEndReason(), "%s", pConn->GetConnectionEndDebugString() );
					pConn->ConnectionQueueDestroy();
					return true;

				case k_EGameNetworkingConnectionState_Connecting:

					// If they returned null, that means they want to totally ignore it.
					if ( !pConn->m_pSignaling )
					{
						// They decided to ignore it, by just returning null
						SpewVerboseGroup( nLogLevel, "App ignored P2P connect request from %s on vport %d\n",
							GameNetworkingIdentityRender( pConn->m_identityRemote ).c_str(), nLocalVirtualPort );
						pConn->ConnectionQueueDestroy();
						return true;
					}

					// They return signaling, which means that they will consider accepting it.
					// But they didn't accept it, so they want to go through the normal
					// callback mechanism.

					SpewVerboseGroup( nLogLevel, "[%s] Received incoming P2P connect request; awaiting app to accept connection\n",
						pConn->GetDescription() );
					pConn->PostConnectionStateChangedCallback( k_EGameNetworkingConnectionState_None, k_EGameNetworkingConnectionState_Connecting );
					break;

				case k_EGameNetworkingConnectionState_Connected:
					AssertMsg( false, "How did we already get connected?  We should be finding route?");
				case k_EGameNetworkingConnectionState_FindingRoute:
					// They accepted the request already.
					break;
			}

			// Stop suppressing state change notifications
			Assert( pConn->m_nSupressStateChangeCallbacks == 1 );
			pConn->m_nSupressStateChangeCallbacks = 0;
		}
	}

	// Process the message
	return pConn->ProcessSignal( msg, usecNow );
}

} // namespace GameNetworkingSocketsLib

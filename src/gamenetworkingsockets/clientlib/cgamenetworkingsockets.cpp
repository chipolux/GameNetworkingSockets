//====== Copyright Valve Corporation, All rights reserved. ====================

#include "cgamenetworkingsockets.h"
#include "gamenetworkingsockets_lowlevel.h"
#include "gamenetworkingsockets_connections.h"
#include "gamenetworkingsockets_udp.h"
#include "../gamenetworkingsockets_certstore.h"
#include "crypto.h"

#ifdef STEAMNETWORKINGSOCKETS_STANDALONELIB
#include <gns/gamenetworkingsockets.h>
#endif

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES
#include "cgamenetworkingmessages.h"
#endif

// Needed for the platform checks below
#if defined(__APPLE__)
	#include "AvailabilityMacros.h"
	#include "TargetConditionals.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IGameNetworkingSockets::~IGameNetworkingSockets() {}
IGameNetworkingUtils::~IGameNetworkingUtils() {}

// Put everything in a namespace, so we don't violate the one definition rule
namespace GameNetworkingSocketsLib {

/////////////////////////////////////////////////////////////////////////////
//
// Configuration Variables
//
/////////////////////////////////////////////////////////////////////////////

DEFINE_GLOBAL_CONFIGVAL( float, FakePacketLoss_Send, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( float, FakePacketLoss_Recv, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( int32, FakePacketLag_Send, 0, 0, 5000 );
DEFINE_GLOBAL_CONFIGVAL( int32, FakePacketLag_Recv, 0, 0, 5000 );
DEFINE_GLOBAL_CONFIGVAL( float, FakePacketReorder_Send, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( float, FakePacketReorder_Recv, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( int32, FakePacketReorder_Time, 15, 0, 5000 );
DEFINE_GLOBAL_CONFIGVAL( float, FakePacketDup_Send, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( float, FakePacketDup_Recv, 0.0f, 0.0f, 100.0f );
DEFINE_GLOBAL_CONFIGVAL( int32, FakePacketDup_TimeMax, 10, 0, 5000 );
DEFINE_GLOBAL_CONFIGVAL( int32, PacketTraceMaxBytes, -1, -1, 99999 );
DEFINE_GLOBAL_CONFIGVAL( int32, FakeRateLimit_Send_Rate, 0, 0, 1024*1024*1024 );
DEFINE_GLOBAL_CONFIGVAL( int32, FakeRateLimit_Send_Burst, 16*1024, 0, 1024*1024 );
DEFINE_GLOBAL_CONFIGVAL( int32, FakeRateLimit_Recv_Rate, 0, 0, 1024*1024*1024 );
DEFINE_GLOBAL_CONFIGVAL( int32, FakeRateLimit_Recv_Burst, 16*1024, 0, 1024*1024 );

DEFINE_GLOBAL_CONFIGVAL( int32, EnumerateDevVars, 0, 0, 1 );

DEFINE_GLOBAL_CONFIGVAL( void *, Callback_AuthStatusChanged, nullptr );
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES
DEFINE_GLOBAL_CONFIGVAL( void*, Callback_MessagesSessionRequest, nullptr );
DEFINE_GLOBAL_CONFIGVAL( void*, Callback_MessagesSessionFailed, nullptr );
#endif
DEFINE_GLOBAL_CONFIGVAL( void *, Callback_CreateConnectionSignaling, nullptr );

DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, TimeoutInitial, 10000, 0, INT32_MAX );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, TimeoutConnected, 10000, 0, INT32_MAX );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, SendBufferSize, 512*1024, 0, 0x10000000 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int64, ConnectionUserData, -1 ); // no limits here
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, SendRateMin, 128*1024, 1024, 0x10000000 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, SendRateMax, 1024*1024, 1024, 0x10000000 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, NagleTime, 5000, 0, 20000 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, MTU_PacketSize, 1300, k_cbGameNetworkingSocketsMinMTUPacketSize, k_cbGameNetworkingSocketsMaxUDPMsgLen );
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
	// We don't have a trusted third party, so allow this by default,
	// and don't warn about it
	DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, IP_AllowWithoutAuth, 2, 0, 2 );
#else
	DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, IP_AllowWithoutAuth, 0, 0, 2 );
#endif
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, Unencrypted, 0, 0, 3 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, SymmetricConnect, 0, 0, 1 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LocalVirtualPort, -1, -1, 65535 );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LogLevel_AckRTT, k_EGameNetworkingSocketsDebugOutputType_Warning, k_EGameNetworkingSocketsDebugOutputType_Error, k_EGameNetworkingSocketsDebugOutputType_Everything );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LogLevel_PacketDecode, k_EGameNetworkingSocketsDebugOutputType_Warning, k_EGameNetworkingSocketsDebugOutputType_Error, k_EGameNetworkingSocketsDebugOutputType_Everything );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LogLevel_Message, k_EGameNetworkingSocketsDebugOutputType_Warning, k_EGameNetworkingSocketsDebugOutputType_Error, k_EGameNetworkingSocketsDebugOutputType_Everything );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LogLevel_PacketGaps, k_EGameNetworkingSocketsDebugOutputType_Warning, k_EGameNetworkingSocketsDebugOutputType_Error, k_EGameNetworkingSocketsDebugOutputType_Everything );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, LogLevel_P2PRendezvous, k_EGameNetworkingSocketsDebugOutputType_Warning, k_EGameNetworkingSocketsDebugOutputType_Error, k_EGameNetworkingSocketsDebugOutputType_Everything );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( void *, Callback_ConnectionStatusChanged, nullptr );

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( std::string, P2P_STUN_ServerList, "" );

COMPILE_TIME_ASSERT( k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Default == -1 );
COMPILE_TIME_ASSERT( k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Disable == 0 );
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
	// There is no such thing as "default" if we don't have some sort of platform
	DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, P2P_Transport_ICE_Enable, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_All, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Disable, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_All );
#else
	DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, P2P_Transport_ICE_Enable, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Default, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Default, k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_All );
#endif

DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, P2P_Transport_ICE_Penalty, 0, 0, INT_MAX );
#endif

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( std::string, SDRClient_DebugTicketAddress, "" );
DEFINE_CONNECTON_DEFAULT_CONFIGVAL( int32, P2P_Transport_SDR_Penalty, 0, 0, INT_MAX );
#endif

static GlobalConfigValueEntry *s_pFirstGlobalConfigEntry = nullptr;
static bool s_bConfigValueTableInitted = false;
static std::vector<GlobalConfigValueEntry *> s_vecConfigValueTable; // Sorted by value
static std::vector<GlobalConfigValueEntry *> s_vecConnectionConfigValueTable; // Sorted by offset

GlobalConfigValueEntry::GlobalConfigValueEntry(
	EGameNetworkingConfigValue eValue,
	const char *pszName,
	EGameNetworkingConfigDataType eDataType,
	EGameNetworkingConfigScope eScope,
	int cbOffsetOf
) : m_eValue{ eValue }
, m_pszName{ pszName }
, m_eDataType{ eDataType }
, m_eScope{ eScope }
, m_cbOffsetOf{cbOffsetOf}
, m_pNextEntry( s_pFirstGlobalConfigEntry )
{
	s_pFirstGlobalConfigEntry = this;
	AssertMsg( !s_bConfigValueTableInitted, "Attempt to register more config values after table is already initialized" );
	s_bConfigValueTableInitted = false;
}

static void EnsureConfigValueTableInitted()
{
	if ( s_bConfigValueTableInitted )
		return;
	GameNetworkingGlobalLock scopeLock;
	if ( s_bConfigValueTableInitted )
		return;

	for ( GlobalConfigValueEntry *p = s_pFirstGlobalConfigEntry ; p ; p = p->m_pNextEntry )
	{
		s_vecConfigValueTable.push_back( p );
		if ( p->m_eScope == k_EGameNetworkingConfig_Connection )
			s_vecConnectionConfigValueTable.push_back( p );
	}

	// Sort in ascending order by value, so we can binary search
	std::sort( s_vecConfigValueTable.begin(), s_vecConfigValueTable.end(),
		[]( GlobalConfigValueEntry *a, GlobalConfigValueEntry *b ) { return a->m_eValue < b->m_eValue; } );

	// Sort by struct offset, so that ConnectionConfig::Init will access memory in a sane way.
	// This doesn't really matter, though.
	std::sort( s_vecConnectionConfigValueTable.begin(), s_vecConnectionConfigValueTable.end(),
		[]( GlobalConfigValueEntry *a, GlobalConfigValueEntry *b ) { return a->m_cbOffsetOf < b->m_cbOffsetOf; } );

	// Rebuild linked list, in order, and safety check for duplicates
	int N = len( s_vecConfigValueTable );
	for ( int i = 1 ; i < N ; ++i )
	{
		s_vecConfigValueTable[i-1]->m_pNextEntry = s_vecConfigValueTable[i];
		AssertMsg1( s_vecConfigValueTable[i-1]->m_eValue < s_vecConfigValueTable[i]->m_eValue, "Registered duplicate config value %d", s_vecConfigValueTable[i]->m_eValue );
	}
	s_vecConfigValueTable[N-1]->m_pNextEntry = nullptr;

	s_pFirstGlobalConfigEntry = nullptr;
	s_bConfigValueTableInitted = true;
}

static GlobalConfigValueEntry *FindConfigValueEntry( EGameNetworkingConfigValue eSearchVal )
{
	EnsureConfigValueTableInitted();

	// Binary search
	int l = 0;
	int r = len( s_vecConfigValueTable )-1;
	while ( l <= r )
	{
		int m = (l+r)>>1;
		GlobalConfigValueEntry *mp = s_vecConfigValueTable[m];
		if ( eSearchVal < mp->m_eValue )
			r = m-1;
		else if ( eSearchVal > mp->m_eValue )
			l = m+1;
		else
			return mp;
	}

	// Not found
	return nullptr;
}

void ConnectionConfig::Init( ConnectionConfig *pInherit )
{
	EnsureConfigValueTableInitted();

	for ( GlobalConfigValueEntry *pEntry : s_vecConnectionConfigValueTable )
	{
		ConfigValueBase *pVal = (ConfigValueBase *)((intptr_t)this + pEntry->m_cbOffsetOf );
		if ( pInherit )
		{
			pVal->m_pInherit = (ConfigValueBase *)((intptr_t)pInherit + pEntry->m_cbOffsetOf );
		}
		else
		{
			// Assume the relevant members are the same, no matter
			// what type T, so just use int32 arbitrarily
			pVal->m_pInherit = &( static_cast< GlobalConfigValueBase<int32> * >( pEntry ) )->m_value;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// Table of active sockets
//
/////////////////////////////////////////////////////////////////////////////

CUtlHashMap<uint16, CGameNetworkConnectionBase *, std::equal_to<uint16>, Identity<uint16> > g_mapConnections;
CUtlHashMap<int, CGameNetworkPollGroup *, std::equal_to<int>, Identity<int> > g_mapPollGroups;
TableLock g_tables_lock;

// Table of active listen sockets.  Listen sockets and this table are protected
// by the global lock.
CUtlHashMap<int, CGameNetworkListenSocketBase *, std::equal_to<int>, Identity<int> > g_mapListenSockets; 

static bool BConnectionStateExistsToAPI( EGameNetworkingConnectionState eState )
{
	switch ( eState )
	{
		default:
			Assert( false );
			return false;
		case k_EGameNetworkingConnectionState_None:
		case k_EGameNetworkingConnectionState_Dead:
		case k_EGameNetworkingConnectionState_FinWait:
		case k_EGameNetworkingConnectionState_Linger:
			return false;

		case k_EGameNetworkingConnectionState_Connecting:
		case k_EGameNetworkingConnectionState_FindingRoute:
		case k_EGameNetworkingConnectionState_Connected:
		case k_EGameNetworkingConnectionState_ClosedByPeer:
		case k_EGameNetworkingConnectionState_ProblemDetectedLocally:
			return true;
	}

}

static CGameNetworkConnectionBase *InternalGetConnectionByHandle( HGameNetConnection sock, ConnectionScopeLock &scopeLock, const char *pszLockTag, bool bForAPI )
{
	if ( sock == 0 )
		return nullptr;
	TableScopeLock tableScopeLock( g_tables_lock );
	int idx = g_mapConnections.Find( uint16( sock ) );
	if ( idx == g_mapConnections.InvalidIndex() )
		return nullptr;
	CGameNetworkConnectionBase *pResult = g_mapConnections[ idx ];
	if ( !pResult )
	{
		AssertMsg( false, "g_mapConnections corruption!" );
		return nullptr;
	}
	if ( uint16( pResult->m_hConnectionSelf ) != uint16( sock ) )
	{
		AssertMsg( false, "Connection map corruption!" );
		return nullptr;
	}

	// Make sure connection is not in the process of being self-destructed
	bool bLocked = false;
	for (;;)
	{

		// Fetch the state of the connection.  This is OK to do
		// even if we don't have the lock.
		EGameNetworkingConnectionState s = pResult->GetState();
		if ( s == k_EGameNetworkingConnectionState_Dead )
			break;
		if ( bForAPI )
		{
			if ( !BConnectionStateExistsToAPI( s ) )
				break;
		}

		// Have we locked already?  Then we're good
		if ( bLocked )
		{
			// NOTE: We unlock the table lock here, OUT OF ORDER!
			return pResult; 
		}

		// State looks good, try to lock the connection.
		// NOTE: we still (briefly) hold the table lock!
		// We *should* be able to totally block here
		// without creating a deadlock, but looping here
		// isn't so bad
		bLocked = scopeLock.TryLock( *pResult->m_pLock, 5, pszLockTag );
	}

	// Connection found in table, but should not be returned to the caller.
	// Unlock the connection, if we locked it
	if ( bLocked )
		scopeLock.Unlock();
	
	return nullptr;
}

CGameNetworkConnectionBase *GetConnectionByHandle( HGameNetConnection sock, ConnectionScopeLock &scopeLock )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();
	return InternalGetConnectionByHandle( sock, scopeLock, nullptr, false );
}

inline CGameNetworkConnectionBase *GetConnectionByHandleForAPI( HGameNetConnection sock, ConnectionScopeLock &scopeLock, const char *pszLockTag )
{
	return InternalGetConnectionByHandle( sock, scopeLock, pszLockTag, true );
}

static CGameNetworkListenSocketBase *GetListenSocketByHandle( HSteamListenSocket sock )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread(); // listen sockets are protected by the global lock!
	if ( sock == k_HSteamListenSocket_Invalid )
		return nullptr;
	AssertMsg( !(sock & 0x80000000), "A poll group handle was used where a listen socket handle was expected" );
	int idx = sock & 0xffff;
	if ( !g_mapListenSockets.IsValidIndex( idx ) )
		return nullptr;
	CGameNetworkListenSocketBase *pResult = g_mapListenSockets[ idx ];
	if ( pResult->m_hListenSocketSelf != sock )
	{
		// Slot was reused, but this handle is now invalid
		return nullptr;
	}
	return pResult;
}

CGameNetworkPollGroup *GetPollGroupByHandle( HGameNetPollGroup hPollGroup, PollGroupScopeLock &scopeLock, const char *pszLockTag )
{
	if ( hPollGroup == k_HGameNetPollGroup_Invalid )
		return nullptr;
	AssertMsg( (hPollGroup & 0x80000000), "A listen socket handle was used where a poll group handle was expected" );
	int idx = hPollGroup & 0xffff;
	TableScopeLock tableScopeLock( g_tables_lock );
	if ( !g_mapPollGroups.IsValidIndex( idx ) )
		return nullptr;
	CGameNetworkPollGroup *pResult = g_mapPollGroups[ idx ];

	// Make sure poll group is the one they really asked for, and also
	// handle deletion race condition
	while ( pResult->m_hPollGroupSelf == hPollGroup )
	{
		if ( scopeLock.TryLock( pResult->m_lock, 1, pszLockTag ) )
			return pResult;
	}

	// Slot was reused, but this handle is now invalid,
	// or poll group deleted race condition
	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSteamSocketNetworkingBase
//
/////////////////////////////////////////////////////////////////////////////

std::vector<CGameNetworkingSockets *> CGameNetworkingSockets::s_vecGameNetworkingSocketsInstances;

CGameNetworkingSockets::CGameNetworkingSockets( CGameNetworkingUtils *pGameNetworkingUtils )
: m_bHaveLowLevelRef( false )
, m_pGameNetworkingUtils( pGameNetworkingUtils )
, m_pGameNetworkingMessages( nullptr )
, m_bEverTriedToGetCert( false )
, m_bEverGotCert( false )
#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
, m_scheduleCheckRenewCert( this, &CGameNetworkingSockets::CheckAuthenticationPrerequisites )
#endif
, m_mutexPendingCallbacks( "pending_callbacks" )
{
	m_connectionConfig.Init( nullptr );
	InternalInitIdentity();
}

void CGameNetworkingSockets::InternalInitIdentity()
{
	m_identity.Clear();
	m_msgSignedCert.Clear();
	m_msgCert.Clear();
	m_keyPrivateKey.Wipe();

	#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
		m_CertStatus.m_eAvail = k_EGameNetworkingAvailability_NeverTried;
		m_CertStatus.m_debugMsg[0] = '\0';
	#else
		m_CertStatus.m_eAvail = k_EGameNetworkingAvailability_CannotTry;
		V_strcpy_safe( m_CertStatus.m_debugMsg, "No certificate authority" );
	#endif
	m_AuthenticationStatus = m_CertStatus;
	m_bEverTriedToGetCert = false;
	m_bEverGotCert = false;
}

CGameNetworkingSockets::~CGameNetworkingSockets()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();
	Assert( !m_bHaveLowLevelRef ); // Called destructor directly?  Use Destroy()!
}

#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
bool CGameNetworkingSockets::BInitGameNetworkingSockets( const GameNetworkingIdentity *pIdentity, SteamDatagramErrMsg &errMsg )
{
	AssertMsg( !m_bHaveLowLevelRef, "Initted interface twice?" );

	// Make sure low level socket support is ready
	if ( !BInitLowLevel( errMsg ) )
		return false;

	if ( pIdentity )
		m_identity = *pIdentity;
	else
		CacheIdentity();

	return true;
}
#endif

bool CGameNetworkingSockets::BInitLowLevel( GameNetworkingErrMsg &errMsg )
{
	if ( m_bHaveLowLevelRef )
		return true;
	if ( !BGameNetworkingSocketsLowLevelAddRef( errMsg) )
		return false;

	// Add us to list of extant instances only after we have done some initialization
	if ( !has_element( s_vecGameNetworkingSocketsInstances, this ) )
		s_vecGameNetworkingSocketsInstances.push_back( this );

	m_bHaveLowLevelRef = true;
	return true;
}

void CGameNetworkingSockets::KillConnections()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CGameNetworkingSockets::KillConnections" );
	TableScopeLock tableScopeLock( g_tables_lock );

	// Warn messages interface that it needs to clean up.  We need to do this
	// because that class has pointers to objects that we are about to destroy.
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES
		if ( m_pGameNetworkingMessages )
			m_pGameNetworkingMessages->FreeResources();
	#endif

	// Destroy all of my connections
	CGameNetworkConnectionBase::ProcessDeletionList();
	FOR_EACH_HASHMAP( g_mapConnections, idx )
	{
		CGameNetworkConnectionBase *pConn = g_mapConnections[idx];
		if ( pConn->m_pGameNetworkingSocketsInterface == this )
		{
			ConnectionScopeLock connectionLock( *pConn );
			pConn->ConnectionQueueDestroy();
		}
	}
	CGameNetworkConnectionBase::ProcessDeletionList();

	// Destroy all of my listen sockets
	FOR_EACH_HASHMAP( g_mapListenSockets, idx )
	{
		CGameNetworkListenSocketBase *pSock = g_mapListenSockets[idx];
		if ( pSock->m_pGameNetworkingSocketsInterface == this )
		{
			DbgVerify( CloseListenSocket( pSock->m_hListenSocketSelf ) );
			Assert( !g_mapListenSockets.IsValidIndex( idx ) );
		}
	}

	// Destroy all of my poll groups
	FOR_EACH_HASHMAP( g_mapPollGroups, idx )
	{
		CGameNetworkPollGroup *pPollGroup = g_mapPollGroups[idx];
		if ( pPollGroup->m_pGameNetworkingSocketsInterface == this )
		{
			DbgVerify( DestroyPollGroup( pPollGroup->m_hPollGroupSelf ) );
			Assert( !g_mapPollGroups.IsValidIndex( idx ) );
		}
	}
}

void CGameNetworkingSockets::Destroy()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CGameNetworkingSockets::Destroy" );

	FreeResources();

	// Nuke messages interface, if we had one.
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES
		if ( m_pGameNetworkingMessages )
		{
			delete m_pGameNetworkingMessages;
			Assert( m_pGameNetworkingMessages == nullptr ); // Destructor should sever this link
			m_pGameNetworkingMessages = nullptr; // Buuuuut we'll slam it, too, in case there's a bug
		}
	#endif

	// Remove from list of extant instances, if we are there
	find_and_remove_element( s_vecGameNetworkingSocketsInstances, this );

	delete this;
}

void CGameNetworkingSockets::FreeResources()
{

	KillConnections();

	// Clear identity and crypto stuff.
	// If we are re-initialized, we might get new ones
	InternalInitIdentity();

	// Mark us as no longer being setup
	if ( m_bHaveLowLevelRef )
	{
		m_bHaveLowLevelRef = false;
		GameNetworkingSocketsLowLevelDecRef();
	}
}

bool CGameNetworkingSockets::BHasAnyConnections() const
{
	TableScopeLock tableScopeLock( g_tables_lock );
	for ( CGameNetworkConnectionBase *pConn: g_mapConnections.IterValues() )
	{
		if ( pConn->m_pGameNetworkingSocketsInterface == this )
			return true;
	}
	return false;
}

bool CGameNetworkingSockets::BHasAnyListenSockets() const
{
	TableScopeLock tableScopeLock( g_tables_lock );
	for ( CGameNetworkListenSocketBase *pSock: g_mapListenSockets.IterValues() )
	{
		if ( pSock->m_pGameNetworkingSocketsInterface == this )
			return true;
	}
	return false;
}

bool CGameNetworkingSockets::GetIdentity( GameNetworkingIdentity *pIdentity )
{
	GameNetworkingGlobalLock scopeLock( "GetIdentity" );
	InternalGetIdentity();
	if ( pIdentity )
		*pIdentity = m_identity;
	return !m_identity.IsInvalid();
}

int CGameNetworkingSockets::GetSecondsUntilCertExpiry() const
{
	if ( !m_msgSignedCert.has_cert() )
		return INT_MIN;

	Assert( m_msgSignedCert.has_ca_signature() ); // Connections may use unsigned certs in certain situations, but we never use them here
	Assert( m_msgCert.has_key_data() );
	Assert( m_msgCert.has_time_expiry() ); // We should never generate keys without an expiry!

	int nSeconduntilExpiry = (long)m_msgCert.time_expiry() - (long)m_pGameNetworkingUtils->GetTimeSecure();
	return nSeconduntilExpiry;
}

bool CGameNetworkingSockets::GetCertificateRequest( int *pcbBlob, void *pBlob, GameNetworkingErrMsg &errMsg )
{
	GameNetworkingGlobalLock scopeLock( "GetCertificateRequest" );

	// If we don't have a private key, generate one now.
	CECSigningPublicKey pubKey;
	if ( m_keyPrivateKey.IsValid() )
	{
		DbgVerify( m_keyPrivateKey.GetPublicKey( &pubKey ) );
	}
	else
	{
		CCrypto::GenerateSigningKeyPair( &pubKey, &m_keyPrivateKey );
	}

	// Fill out the request
	CMsgSteamDatagramCertificateRequest msgRequest;
	CMsgSteamDatagramCertificate &msgCert =*msgRequest.mutable_cert();

	// Our public key
	msgCert.set_key_type( CMsgSteamDatagramCertificate_EKeyType_ED25519 );
	DbgVerify( pubKey.GetRawDataAsStdString( msgCert.mutable_key_data() ) );

	// Our identity, if we know it
	InternalGetIdentity();
	if ( !m_identity.IsInvalid() && !m_identity.IsLocalHost() )
	{
		GameNetworkingIdentityToProtobuf( m_identity, msgCert, identity_string, legacy_identity_binary, legacy_game_id );
	}

	// Check size
	int cb = ProtoMsgByteSize( msgRequest );
	if ( !pBlob )
	{
		*pcbBlob = cb;
		return true;
	}
	if ( cb > *pcbBlob )
	{
		*pcbBlob = cb;
		V_sprintf_safe( errMsg, "%d byte buffer not big enough; %d bytes required", *pcbBlob, cb );
		return false;
	}

	*pcbBlob = cb;
	uint8 *p = (uint8 *)pBlob;
	DbgVerify( msgRequest.SerializeWithCachedSizesToArray( p ) == p + cb );
	return true;
}

bool CGameNetworkingSockets::SetCertificate( const void *pCertificate, int cbCertificate, GameNetworkingErrMsg &errMsg )
{
	// Crack the blob
	CMsgSteamDatagramCertificateSigned msgCertSigned;
	if ( !msgCertSigned.ParseFromArray( pCertificate, cbCertificate ) )
	{
		V_strcpy_safe( errMsg, "CMsgSteamDatagramCertificateSigned failed protobuf parse" );
		return false;
	}

	GameNetworkingGlobalLock scopeLock( "SetCertificate" );

	// Crack the cert, and check the signature.  If *we* aren't even willing
	// to trust it, assume that our peers won't either
	CMsgSteamDatagramCertificate msgCert;
	time_t authTime = m_pGameNetworkingUtils->GetTimeSecure();
	const CertAuthScope *pAuthScope = CertStore_CheckCert( msgCertSigned, msgCert, authTime, errMsg );
	if ( !pAuthScope )
	{
		SpewWarning( "SetCertificate: We are not currently able to verify our own cert!  %s.  Continuing anyway!", errMsg );
	}

	// Extract the identity from the cert
	GameNetworkingErrMsg tempErrMsg;
	GameNetworkingIdentity certIdentity;
	int r = GameNetworkingIdentityFromCert( certIdentity, msgCert, tempErrMsg );
	if ( r < 0 )
	{
		V_sprintf_safe( errMsg, "Cert has invalid identity.  %s", tempErrMsg );
		return false;
	}

	// We currently only support one key type
	if ( msgCert.key_type() != CMsgSteamDatagramCertificate_EKeyType_ED25519 || msgCert.key_data().size() != 32 )
	{
		V_strcpy_safe( errMsg, "Cert has invalid public key" );
		return false;
	}

	// Does cert contain a private key?
	if ( msgCertSigned.has_private_key_data() )
	{
		// The degree to which the key is actually "private" is not
		// really known to us.  However there are some use cases where
		// we will accept a cert 
		const std::string &private_key_data = msgCertSigned.private_key_data();
		if ( m_keyPrivateKey.IsValid() )
		{

			// We already chose a private key, so the cert must match.
			// For the most common use cases, we choose a private
			// key and it never leaves the current process.
			if ( m_keyPrivateKey.GetRawDataSize() != private_key_data.length()
				|| memcmp( m_keyPrivateKey.GetRawDataPtr(), private_key_data.c_str(), private_key_data.length() ) != 0 )
			{
				V_strcpy_safe( errMsg, "Private key mismatch" );
				return false;
			}
		}
		else
		{
			// We haven't chosen a private key yet, so we'll accept this one.
			if ( !m_keyPrivateKey.SetRawDataFromStdString( private_key_data ) )
			{
				V_strcpy_safe( errMsg, "Invalid private key" );
				return false;
			}
		}
	}
	else if ( !m_keyPrivateKey.IsValid() )
	{
		// WAT
		V_strcpy_safe( errMsg, "Cannot set cert.  No private key?" );
		return false;
	}

	// Make sure the cert actually matches our public key.
	if ( memcmp( msgCert.key_data().c_str(), m_keyPrivateKey.GetPublicKeyRawData(), 32 ) != 0 )
	{
		V_strcpy_safe( errMsg, "Cert public key does not match our private key" );
		return false;
	}

	// Make sure the cert authorizes us for the App we think we are running
	AppId_t nAppID = m_pGameNetworkingUtils->GetAppID();
	if ( !CheckCertAppID( msgCert, pAuthScope, nAppID, tempErrMsg ) )
	{
		V_sprintf_safe( errMsg, "Cert does not authorize us for App %u", nAppID );
		return false;
	}

	// If we don't know our identity, then set it now.  Otherwise,
	// it better match.
	if ( m_identity.IsInvalid() || m_identity.IsLocalHost() )
	{
		m_identity = certIdentity;
		SpewMsg( "Local identity established from certificate.  We are '%s'\n", GameNetworkingIdentityRender( m_identity ).c_str() );
	}
	else if ( !( m_identity == certIdentity ) )
	{
		V_sprintf_safe( errMsg, "Cert is for identity '%s'.  We are '%s'", GameNetworkingIdentityRender( certIdentity ).c_str(), GameNetworkingIdentityRender( m_identity ).c_str() );
		return false;
	}

	// Save it off
	m_msgSignedCert = std::move( msgCertSigned );
	m_msgCert = std::move( msgCert );
	// If shouldn't already be expired.
	AssertMsg( GetSecondsUntilCertExpiry() > 0, "Cert already invalid / expired?" );

	// We've got a valid cert
	SetCertStatus( k_EGameNetworkingAvailability_Current, "OK" );

	// Make sure we have everything else we need to do authentication.
	// This will also make sure we have renewal scheduled
	AuthenticationNeeded();

	// OK
	return true;
}

void CGameNetworkingSockets::ResetIdentity( const GameNetworkingIdentity *pIdentity )
{
#ifdef STEAMNETWORKINGSOCKETS_STEAM
	Assert( !"Not supported on game" );
#else
	KillConnections();
	InternalInitIdentity();
	if ( pIdentity )
		m_identity = *pIdentity;
#endif
}

EGameNetworkingAvailability CGameNetworkingSockets::InitAuthentication()
{
	GameNetworkingGlobalLock scopeLock( "InitAuthentication" );

	// Check/fetch prerequisites
	AuthenticationNeeded();

	// Return status
	return m_AuthenticationStatus.m_eAvail;
}

void CGameNetworkingSockets::CheckAuthenticationPrerequisites( GameNetworkingMicroseconds usecNow )
{
#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	// Check if we're in flight already.
	bool bInFlight = BCertRequestInFlight();

	// Do we already have a cert?
	if ( m_msgSignedCert.has_cert() )
	{
		//Assert( m_CertStatus.m_eAvail == k_EGameNetworkingAvailability_Current );

		// How much more life does it have in it?
		int nSeconduntilExpiry = GetSecondsUntilCertExpiry();
		if ( nSeconduntilExpiry < 0 )
		{

			// It's already expired, we might as well discard it now.
			SpewMsg( "Cert expired %d seconds ago.  Discarding and requesting another\n", -nSeconduntilExpiry );
			m_msgSignedCert.Clear();
			m_msgCert.Clear();
			m_keyPrivateKey.Wipe();

			// Update cert status
			SetCertStatus( k_EGameNetworkingAvailability_Previously, "Expired" );
		}
		else
		{

			// If request is already active, don't do any of the work below, and don't spam while we wait, since this function may be called frequently.
			if ( bInFlight )
				return;

			// Check if it's time to renew
			GameNetworkingMicroseconds usecTargetRenew = usecNow + ( nSeconduntilExpiry - k_nSecCertExpirySeekRenew ) * k_nMillion;
			if ( usecTargetRenew > usecNow )
			{
				GameNetworkingMicroseconds usecScheduledRenew = m_scheduleCheckRenewCert.GetScheduleTime();
				GameNetworkingMicroseconds usecLatestRenew = usecTargetRenew + 4*k_nMillion;
				if ( usecScheduledRenew <= usecLatestRenew )
				{
					// Currently scheduled time is good enough.  Don't constantly update the schedule time,
					// that involves a (small amount) of work.  Just wait for it
				}
				else
				{
					// Schedule a check later
					m_scheduleCheckRenewCert.Schedule( usecTargetRenew + 2*k_nMillion );
				}
				return;
			}

			// Currently valid, but it's time to renew.  Spew about this.
			SpewMsg( "Cert expires in %d seconds.  Requesting another, but keeping current cert in case request fails\n", nSeconduntilExpiry );
		}
	}

	// If a request is already active, then we just need to wait for it to complete
	if ( bInFlight )
		return;

	// Invoke platform code to begin fetching a cert
	BeginFetchCertAsync();
#endif
}

void CGameNetworkingSockets::SetCertStatus( EGameNetworkingAvailability eAvail, const char *pszFmt, ... )
{
	char msg[ sizeof(m_CertStatus.m_debugMsg) ];
	va_list ap;
	va_start( ap, pszFmt );
	V_vsprintf_safe( msg, pszFmt, ap );
	va_end( ap );

	// Mark success or an attempt
	if ( eAvail == k_EGameNetworkingAvailability_Current )
		m_bEverGotCert = true;
	if ( eAvail == k_EGameNetworkingAvailability_Attempting || eAvail == k_EGameNetworkingAvailability_Retrying )
		m_bEverTriedToGetCert = true;

	// If we failed, but we previously succeeded, convert to "previously"
	if ( eAvail == k_EGameNetworkingAvailability_Failed && m_bEverGotCert )
		eAvail = k_EGameNetworkingAvailability_Previously;

	// No change?
	if ( m_CertStatus.m_eAvail == eAvail && V_stricmp( m_CertStatus.m_debugMsg, msg ) == 0 )
		return;

	// Update
	m_CertStatus.m_eAvail = eAvail;
	V_strcpy_safe( m_CertStatus.m_debugMsg, msg );

	// Check if our high level authentication status changed
	DeduceAuthenticationStatus();
}

void CGameNetworkingSockets::DeduceAuthenticationStatus()
{
	// For the base class, the overall authentication status is identical to the status of
	// our cert.  (Derived classes may add additional criteria)
	SetAuthenticationStatus( m_CertStatus );
}

void CGameNetworkingSockets::SetAuthenticationStatus( const GameNetAuthenticationStatus_t &newStatus )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	// No change?
	bool bStatusChanged = newStatus.m_eAvail != m_AuthenticationStatus.m_eAvail;
	if ( !bStatusChanged && V_strcmp( m_AuthenticationStatus.m_debugMsg, newStatus.m_debugMsg ) == 0 )
		return;

	// Update
	m_AuthenticationStatus = newStatus;

	// Re-cache identity
	InternalGetIdentity();

	// Post a callback, but only if the high level status changed.  Don't post a callback just
	// because the message changed
	if ( bStatusChanged )
	{
		// Spew
		SpewMsg( "AuthStatus (%s):  %s  (%s)",
			GameNetworkingIdentityRender( m_identity ).c_str(),
			GetAvailabilityString( m_AuthenticationStatus.m_eAvail ), m_AuthenticationStatus.m_debugMsg );

		QueueCallback( m_AuthenticationStatus, g_Config_Callback_AuthStatusChanged.Get() );
	}
}

#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
void CGameNetworkingSockets::AsyncCertRequestFinished()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "AsyncCertRequestFinished" );

	Assert( m_msgSignedCert.has_cert() );
	SetCertStatus( k_EGameNetworkingAvailability_Current, "OK" );

	// Check for any connections that we own that are waiting on a cert
	TableScopeLock tableScopeLock( g_tables_lock );
	for ( CGameNetworkConnectionBase *pConn: g_mapConnections.IterValues() )
	{
		if ( pConn->m_pGameNetworkingSocketsInterface == this )
			pConn->InterfaceGotCert();
	}
}

void CGameNetworkingSockets::CertRequestFailed( EGameNetworkingAvailability eCertAvail, EGameNetConnectionEnd nConnectionEndReason, const char *pszMsg )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CertRequestFailed" );

	SpewWarning( "Cert request for %s failed with reason code %d.  %s\n", GameNetworkingIdentityRender( InternalGetIdentity() ).c_str(), nConnectionEndReason, pszMsg );

	// Schedule a retry.  Note that if we have active connections that need for a cert,
	// we may end up retrying sooner.  If we don't have any active connections, spamming
	// retries way too frequently may be really bad; we might end up DoS-ing ourselves.
	// Do we need to make this configurable?
	m_scheduleCheckRenewCert.Schedule( GameNetworkingSockets_GetLocalTimestamp() + k_nMillion*30 );

	if ( m_msgSignedCert.has_cert() )
	{
		SpewMsg( "But we still have a valid cert, continuing with that one\n" );
		AsyncCertRequestFinished();
		return;
	}

	// Set generic cert status, so we will post a callback
	SetCertStatus( eCertAvail, "%s", pszMsg );

	TableScopeLock tableScopeLock( g_tables_lock );
	for ( CGameNetworkConnectionBase *pConn: g_mapConnections.IterValues() )
	{
		if ( pConn->m_pGameNetworkingSocketsInterface == this )
			pConn->CertRequestFailed( nConnectionEndReason, pszMsg );
	}

	// FIXME If we have any listen sockets, we might need to let them know about this as well?
}
#endif

EGameNetworkingAvailability CGameNetworkingSockets::GetAuthenticationStatus( GameNetAuthenticationStatus_t *pDetails )
{
	GameNetworkingGlobalLock scopeLock; // !SPEED! We could protect this with a more tightly scoped lock, if we think this is eomthing people might be polling

	// Return details, if requested
	if ( pDetails )
		*pDetails = m_AuthenticationStatus;

	// Return status
	return m_AuthenticationStatus.m_eAvail;
}

HSteamListenSocket CGameNetworkingSockets::CreateListenSocketIP( const GameNetworkingIPAddr &localAddr, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	GameNetworkingGlobalLock scopeLock( "CreateListenSocketIP" );
	SteamDatagramErrMsg errMsg;

	CGameNetworkListenSocketDirectUDP *pSock = new CGameNetworkListenSocketDirectUDP( this );
	if ( !pSock )
		return k_HSteamListenSocket_Invalid;
	if ( !pSock->BInit( localAddr, nOptions, pOptions, errMsg ) )
	{
		SpewError( "Cannot create listen socket.  %s", errMsg );
		pSock->Destroy();
		return k_HSteamListenSocket_Invalid;
	}

	return pSock->m_hListenSocketSelf;
}

HGameNetConnection CGameNetworkingSockets::ConnectByIPAddress( const GameNetworkingIPAddr &address, int nOptions, const GameNetworkingConfigValue_t *pOptions )
{
	GameNetworkingGlobalLock scopeLock( "ConnectByIPAddress" );
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionUDP *pConn = new CGameNetworkConnectionUDP( this, connectionLock );
	if ( !pConn )
		return k_HGameNetConnection_Invalid;
	SteamDatagramErrMsg errMsg;
	if ( !pConn->BInitConnect( address, nOptions, pOptions, errMsg ) )
	{
		SpewError( "Cannot create IPv4 connection.  %s", errMsg );
		pConn->ConnectionQueueDestroy();
		return k_HGameNetConnection_Invalid;
	}

	return pConn->m_hConnectionSelf;
}


EResult CGameNetworkingSockets::AcceptConnection( HGameNetConnection hConn )
{
	GameNetworkingGlobalLock scopeLock( "AcceptConnection" ); // Take global lock, since this will lead to connection state transition
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, nullptr );
	if ( !pConn )
	{
		SpewError( "Cannot accept connection #%u; invalid connection handle", hConn );
		return k_EResultInvalidParam;
	}

	// Accept it
	return pConn->APIAcceptConnection();
}

bool CGameNetworkingSockets::CloseConnection( HGameNetConnection hConn, int nReason, const char *pszDebug, bool bEnableLinger )
{
	GameNetworkingGlobalLock scopeLock( "CloseConnection" ); // Take global lock, we are going to change connection state and/or destroy objects
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, nullptr );
	if ( !pConn )
		return false;

	// Close it
	pConn->APICloseConnection( nReason, pszDebug, bEnableLinger );
	return true;
}

bool CGameNetworkingSockets::CloseListenSocket( HSteamListenSocket hSocket )
{
	GameNetworkingGlobalLock scopeLock( "CloseListenSocket" ); // Take global lock, we are going to destroy objects
	CGameNetworkListenSocketBase *pSock = GetListenSocketByHandle( hSocket );
	if ( !pSock )
		return false;

	// Delete the socket itself
	// NOTE: If you change this, look at CSteamSocketNetworking::Kill()!
	pSock->Destroy();
	return true;
}

bool CGameNetworkingSockets::SetConnectionUserData( HGameNetConnection hPeer, int64 nUserData )
{
	//GameNetworkingGlobalLock scopeLock( "SetConnectionUserData" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hPeer, connectionLock, "SetConnectionUserData" );
	if ( !pConn )
		return false;
	pConn->SetUserData( nUserData );
	return true;
}

int64 CGameNetworkingSockets::GetConnectionUserData( HGameNetConnection hPeer )
{
	//GameNetworkingGlobalLock scopeLock( "GetConnectionUserData" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hPeer, connectionLock, "GetConnectionUserData" );
	if ( !pConn )
		return -1;
	return pConn->GetUserData();
}

void CGameNetworkingSockets::SetConnectionName( HGameNetConnection hPeer, const char *pszName )
{
	GameNetworkingGlobalLock scopeLock( "SetConnectionName" ); // NOTE: Yes, we must take global lock for this.  See CGameNetworkConnectionBase::SetDescription
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hPeer, connectionLock, nullptr );
	if ( !pConn )
		return;
	pConn->SetAppName( pszName );
}

bool CGameNetworkingSockets::GetConnectionName( HGameNetConnection hPeer, char *pszName, int nMaxLen )
{
	//GameNetworkingGlobalLock scopeLock( "GetConnectionName" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hPeer, connectionLock, "GetConnectionName" );
	if ( !pConn )
		return false;
	V_strncpy( pszName, pConn->GetAppName(), nMaxLen );
	return true;
}

EResult CGameNetworkingSockets::SendMessageToConnection( HGameNetConnection hConn, const void *pData, uint32 cbData, int nSendFlags, int64 *pOutMessageNumber )
{
	//GameNetworkingGlobalLock scopeLock( "SendMessageToConnection" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "SendMessageToConnection" );
	if ( !pConn )
		return k_EResultInvalidParam;
	return pConn->APISendMessageToConnection( pData, cbData, nSendFlags, pOutMessageNumber );
}

void CGameNetworkingSockets::SendMessages( int nMessages, GameNetworkingMessage_t *const *pMessages, int64 *pOutMessageNumberOrResult )
{

	// Get list of messages, grouped by connection.
	// But within the connection, it is important that we
	// keep them in the same order!
	struct SortMsg_t
	{
		HGameNetConnection m_hConn;
		int m_idx;
		inline bool operator<(const SortMsg_t &x ) const
		{
			if ( m_hConn < x.m_hConn ) return true;
			if ( m_hConn > x.m_hConn ) return false;
			return m_idx < x.m_idx;
		}
	};
	SortMsg_t *pSortMessages = (SortMsg_t *)alloca( nMessages * sizeof(SortMsg_t) );
	int nSortMessages = 0;

	for ( int i = 0 ; i < nMessages ; ++i )
	{

		// Sanity check that message is valid
		CGameNetworkingMessage *pMsg = static_cast<CGameNetworkingMessage*>( pMessages[i] );
		if ( !pMsg )
		{
			if ( pOutMessageNumberOrResult )
				pOutMessageNumberOrResult[i] = -k_EResultInvalidParam;
			continue;
		}

		if ( pMsg->m_conn == k_HGameNetConnection_Invalid )
		{
			if ( pOutMessageNumberOrResult )
				pOutMessageNumberOrResult[i] = -k_EResultInvalidParam;
			pMsg->Release();
			continue;
		}

		pSortMessages[ nSortMessages ].m_hConn = pMsg->m_conn;
		pSortMessages[ nSortMessages ].m_idx = i;
		++nSortMessages;
	}

	if ( nSortMessages < 1 )
		return;

	SortMsg_t *const pSortEnd = pSortMessages+nSortMessages;
	std::sort( pSortMessages, pSortEnd );

	// OK, we are ready to begin

	// GameNetworkingGlobalLock scopeLock( "SendMessages" ); // NO, not necessary!
	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	CGameNetworkConnectionBase *pConn = nullptr;
	HGameNetConnection hConn = k_HGameNetConnection_Invalid;
	ConnectionScopeLock connectionLock;
	bool bConnectionThinkImmediately = false;
	for ( SortMsg_t *pSort = pSortMessages ; pSort < pSortEnd ; ++pSort )
	{

		// Switched to a different connection?
		if ( hConn != pSort->m_hConn )
		{

			// Flush out previous connection, if any
			if ( pConn )
			{
				if ( bConnectionThinkImmediately )
					pConn->CheckConnectionStateOrScheduleWakeUp( usecNow );
				connectionLock.Unlock();
				bConnectionThinkImmediately = false;
			}

			// Locate the connection
			hConn = pSort->m_hConn;
			pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "SendMessages" );
		}

		CGameNetworkingMessage *pMsg = static_cast<CGameNetworkingMessage*>( pMessages[pSort->m_idx] );

		// Current connection is valid?
		int64 result;
		if ( pConn )
		{

			// Attempt to send
			bool bThinkImmediately = false;
			result = pConn->APISendMessageToConnection( pMsg, usecNow, &bThinkImmediately );
			if ( bThinkImmediately )
				bConnectionThinkImmediately = true;
		}
		else
		{
			pMsg->Release();
			result = -k_EResultInvalidParam;
		}

		// Return result for this message if they asked for it
		if ( pOutMessageNumberOrResult )
			pOutMessageNumberOrResult[pSort->m_idx] = result;
	}

	// Flush out last connection, if any
	if ( bConnectionThinkImmediately )
		pConn->CheckConnectionStateOrScheduleWakeUp( usecNow );
}

EResult CGameNetworkingSockets::FlushMessagesOnConnection( HGameNetConnection hConn )
{
	//GameNetworkingGlobalLock scopeLock( "FlushMessagesOnConnection" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "FlushMessagesOnConnection" );
	if ( !pConn )
		return k_EResultInvalidParam;
	return pConn->APIFlushMessageOnConnection();
}

int CGameNetworkingSockets::ReceiveMessagesOnConnection( HGameNetConnection hConn, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages )
{
	//GameNetworkingGlobalLock scopeLock( "ReceiveMessagesOnConnection" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "ReceiveMessagesOnConnection" );
	if ( !pConn )
		return -1;
	return pConn->APIReceiveMessages( ppOutMessages, nMaxMessages );
}

HGameNetPollGroup CGameNetworkingSockets::CreatePollGroup()
{
	GameNetworkingGlobalLock scopeLock( "CreatePollGroup" ); // Take global lock, because we will be creating objects
	PollGroupScopeLock pollGroupScopeLock;
	CGameNetworkPollGroup *pPollGroup = InternalCreatePollGroup( pollGroupScopeLock );
	return pPollGroup->m_hPollGroupSelf;
}

CGameNetworkPollGroup *CGameNetworkingSockets::InternalCreatePollGroup( PollGroupScopeLock &scopeLock )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();
	TableScopeLock tableScopeLock( g_tables_lock );
	CGameNetworkPollGroup *pPollGroup = new CGameNetworkPollGroup( this );
	scopeLock.Lock( pPollGroup->m_lock );
	pPollGroup->AssignHandleAndAddToGlobalTable();
	return pPollGroup;
}

bool CGameNetworkingSockets::DestroyPollGroup( HGameNetPollGroup hPollGroup )
{
	GameNetworkingGlobalLock scopeLock( "DestroyPollGroup" ); // Take global lock, since we'll be destroying objects
	TableScopeLock tableScopeLock( g_tables_lock ); // We'll need to be able to remove the poll group from the tables list
	PollGroupScopeLock pollGroupLock;
	CGameNetworkPollGroup *pPollGroup = GetPollGroupByHandle( hPollGroup, pollGroupLock, nullptr );
	if ( !pPollGroup )
		return false;
	pollGroupLock.Abandon(); // We're about to destroy the lock itself.  The Destructor will unlock -- we don't want to do it again.
	delete pPollGroup;
	return true;
}

bool CGameNetworkingSockets::SetConnectionPollGroup( HGameNetConnection hConn, HGameNetPollGroup hPollGroup )
{
	GameNetworkingGlobalLock scopeLock( "SetConnectionPollGroup" ); // Take global lock, since we'll need to take multiple object locks
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, nullptr );
	if ( !pConn )
		return false;

	// NOTE: We are allowed to take multiple locks here, in any order, because we have the global
	// lock.  Code that does not hold the global lock may only lock one object at a time

	// Special case for removing the poll group
	if ( hPollGroup == k_HGameNetPollGroup_Invalid )
	{
		pConn->RemoveFromPollGroup();
		return true;
	}


	PollGroupScopeLock pollGroupLock;
	CGameNetworkPollGroup *pPollGroup = GetPollGroupByHandle( hPollGroup, pollGroupLock, nullptr );
	if ( !pPollGroup )
		return false;

	pConn->SetPollGroup( pPollGroup );

	return true;
}

int CGameNetworkingSockets::ReceiveMessagesOnPollGroup( HGameNetPollGroup hPollGroup, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages )
{
	//GameNetworkingGlobalLock scopeLock( "ReceiveMessagesOnPollGroup" ); // NO, not necessary!
	PollGroupScopeLock pollGroupLock;
	CGameNetworkPollGroup *pPollGroup = GetPollGroupByHandle( hPollGroup, pollGroupLock, "ReceiveMessagesOnPollGroup" );
	if ( !pPollGroup )
		return -1;
	g_lockAllRecvMessageQueues.lock();
	int nMessagesReceived = pPollGroup->m_queueRecvMessages.RemoveMessages( ppOutMessages, nMaxMessages );
	g_lockAllRecvMessageQueues.unlock();
	return nMessagesReceived;
}

#ifdef STEAMNETWORKINGSOCKETS_STEAMCLIENT
int CGameNetworkingSockets::ReceiveMessagesOnListenSocketLegacyPollGroup( HSteamListenSocket hSocket, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages )
{
	GameNetworkingGlobalLock scopeLock( "ReceiveMessagesOnListenSocket" );
	CGameNetworkListenSocketBase *pSock = GetListenSocketByHandle( hSocket );
	if ( !pSock )
		return -1;
	g_lockAllRecvMessageQueues.lock();
	int nMessagesReceived = pSock->m_legacyPollGroup.m_queueRecvMessages.RemoveMessages( ppOutMessages, nMaxMessages );
	g_lockAllRecvMessageQueues.unlock();
	return nMessagesReceived;
}
#endif

bool CGameNetworkingSockets::GetConnectionInfo( HGameNetConnection hConn, GameNetConnectionInfo_t *pInfo )
{
	//GameNetworkingGlobalLock scopeLock( "GetConnectionInfo" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "GetConnectionInfo" );
	if ( !pConn )
		return false;
	if ( pInfo )
		pConn->ConnectionPopulateInfo( *pInfo );
	return true;
}

bool CGameNetworkingSockets::GetQuickConnectionStatus( HGameNetConnection hConn, GameNetworkingQuickConnectionStatus *pStats )
{
	//GameNetworkingGlobalLock scopeLock( "GetQuickConnectionStatus" ); // NO, not necessary!
	ConnectionScopeLock connectionLock;
	CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, "GetQuickConnectionStatus" );
	if ( !pConn )
		return false;
	if ( pStats )
		pConn->APIGetQuickConnectionStatus( *pStats );
	return true;
}

int CGameNetworkingSockets::GetDetailedConnectionStatus( HGameNetConnection hConn, char *pszBuf, int cbBuf )
{
	GameNetworkingDetailedConnectionStatus stats;

	// Only hold the lock for as long as we need.
	{
		GameNetworkingGlobalLock scopeLock( "GetDetailedConnectionStatus" ); // In some use cases (SDR), we need to touch some shared data structures.  It's easier to just protect this with the global lock than to try to sort that out
		ConnectionScopeLock connectionLock;
		CGameNetworkConnectionBase *pConn = GetConnectionByHandleForAPI( hConn, connectionLock, nullptr );
		if ( !pConn )
			return -1;

		pConn->APIGetDetailedConnectionStatus( stats, GameNetworkingSockets_GetLocalTimestamp() );

	} // Release lock.  We don't need it, and printing can take a while!
	int r = stats.Print( pszBuf, cbBuf );

	/// If just asking for buffer size, pad it a bunch
	/// because connection status can change at any moment.
	if ( r > 0 )
		r += 1024;
	return r;
}

bool CGameNetworkingSockets::GetListenSocketAddress( HSteamListenSocket hSocket, GameNetworkingIPAddr *pAddress )
{
	GameNetworkingGlobalLock scopeLock( "GetListenSocketAddress" );
	CGameNetworkListenSocketBase *pSock = GetListenSocketByHandle( hSocket );
	if ( !pSock )
		return false;
	return pSock->APIGetAddress( pAddress );
}

bool CGameNetworkingSockets::CreateSocketPair( HGameNetConnection *pOutConnection1, HGameNetConnection *pOutConnection2, bool bUseNetworkLoopback, const GameNetworkingIdentity *pIdentity1, const GameNetworkingIdentity *pIdentity2 )
{
	GameNetworkingGlobalLock scopeLock( "CreateSocketPair" );

	// Assume failure
	*pOutConnection1 = k_HGameNetConnection_Invalid;
	*pOutConnection2 = k_HGameNetConnection_Invalid;
	GameNetworkingIdentity identity[2];
	if ( pIdentity1 )
		identity[0] = *pIdentity1;
	else
		identity[0].SetLocalHost();
	if ( pIdentity2 )
		identity[1] = *pIdentity2;
	else
		identity[1].SetLocalHost();

	// Create network connections?
	if ( bUseNetworkLoopback )
	{
		// Create two connection objects
		CGameNetworkConnectionlocalhostLoopback *pConn[2];
		if ( !CGameNetworkConnectionlocalhostLoopback::APICreateSocketPair( this, pConn, identity ) )
			return false;

		// Return their handles
		*pOutConnection1 = pConn[0]->m_hConnectionSelf;
		*pOutConnection2 = pConn[1]->m_hConnectionSelf;
	}
	else
	{
		// Create two connection objects
		CGameNetworkConnectionPipe *pConn[2];
		if ( !CGameNetworkConnectionPipe::APICreateSocketPair( this, pConn, identity ) )
			return false;

		// Return their handles
		*pOutConnection1 = pConn[0]->m_hConnectionSelf;
		*pOutConnection2 = pConn[1]->m_hConnectionSelf;
	}
	return true;
}

bool CGameNetworkingSockets::BCertHasIdentity() const
{
	// We should actually have a cert, otherwise this question cannot be answered
	Assert( m_msgSignedCert.has_cert() );
	Assert( m_msgCert.has_key_data() );
	return m_msgCert.has_identity_string() || m_msgCert.has_legacy_identity_binary() || m_msgCert.has_legacy_game_id();
}


bool CGameNetworkingSockets::SetCertificateAndPrivateKey( const void *pCert, int cbCert, void *pPrivateKey, int cbPrivateKey )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "SetCertificateAndPrivateKey" );

	m_msgCert.Clear();
	m_msgSignedCert.Clear();
	m_keyPrivateKey.Wipe();

	//
	// Decode the private key
	//
	if ( !m_keyPrivateKey.LoadFromAndWipeBuffer( pPrivateKey, cbPrivateKey ) )
	{
		SetCertStatus( k_EGameNetworkingAvailability_Failed, "Invalid private key" );
		return false;
	}

	//
	// Decode the cert
	//
	GameNetworkingErrMsg parseErrMsg;
	if ( !ParseCertFromPEM( pCert, cbCert, m_msgSignedCert, parseErrMsg ) )
	{
		SetCertStatus( k_EGameNetworkingAvailability_Failed, parseErrMsg );
		return false;
	}

	if (
		!m_msgSignedCert.has_cert()
		|| !m_msgCert.ParseFromString( m_msgSignedCert.cert() )
		|| !m_msgCert.has_time_expiry()
		|| !m_msgCert.has_key_data()
	) {
		SetCertStatus( k_EGameNetworkingAvailability_Failed, "Invalid cert" );
		return false;
	}
	if ( m_msgCert.key_type() != CMsgSteamDatagramCertificate_EKeyType_ED25519 )
	{
		SetCertStatus( k_EGameNetworkingAvailability_Failed, "Invalid cert or unsupported public key type" );
		return false;
	}

	//
	// Make sure that the private key and the cert match!
	//

	CECSigningPublicKey pubKey;
	if ( !pubKey.SetRawDataWithoutWipingInput( m_msgCert.key_data().c_str(), m_msgCert.key_data().length() ) )
	{
		SetCertStatus( k_EGameNetworkingAvailability_Failed, "Invalid public key" );
		return false;
	}
	if ( !m_keyPrivateKey.MatchesPublicKey( pubKey ) )
	{
		SetCertStatus( k_EGameNetworkingAvailability_Failed, "Private key doesn't match public key from cert" );
		return false;
	}

	SetCertStatus( k_EGameNetworkingAvailability_Current, "OK" );

	return true;
}

int CGameNetworkingSockets::GetP2P_Transport_ICE_Enable( const GameNetworkingIdentity &identityRemote, int *pOutUserFlags )
{
	// We really shouldn't get here, because this is only a question that makes sense
	// to ask if we have also overridden this function in a derived class, or slammed
	// it before making the connection
	Assert( false );
	if ( pOutUserFlags )
		*pOutUserFlags = 0;
	return k_nGameNetworkingConfig_P2P_Transport_ICE_Enable_Disable;
}

void CGameNetworkingSockets::RunCallbacks()
{

	// Swap into a temp, so that we only hold lock for
	// a brief period.
	std_vector<QueuedCallback> listTemp;
	m_mutexPendingCallbacks.lock();
	listTemp.swap( m_vecPendingCallbacks );
	m_mutexPendingCallbacks.unlock();

	// Dispatch the callbacks
	for ( QueuedCallback &x: listTemp )
	{
		// NOTE: this switch statement is probably not necessary, if we are willing to make
		// some (almost certainly reasonable in practice) assumptions about the parameter
		// passing ABI.  All of these function calls basically have the same signature except
		// for the actual type of the argument being pointed to.

		#define DISPATCH_CALLBACK( structType, fnType ) \
			case structType::k_iCallback: \
				COMPILE_TIME_ASSERT( sizeof(structType) <= sizeof(x.data) ); \
				((fnType)x.fnCallback)( (structType*)x.data ); \
				break; \

		switch ( x.nCallback )
		{
			DISPATCH_CALLBACK( GameNetConnectionStatusChangedCallback_t, FnGameNetConnectionStatusChanged )
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
			DISPATCH_CALLBACK( GameNetAuthenticationStatus_t, FnGameNetAuthenticationStatusChanged )
			DISPATCH_CALLBACK( SteamRelayNetworkStatus_t, FnSteamRelayNetworkStatusChanged )
		#endif
		#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES
			DISPATCH_CALLBACK( GameNetworkingMessagesSessionRequest_t, FnGameNetworkingMessagesSessionRequest )
			DISPATCH_CALLBACK( GameNetworkingMessagesSessionFailed_t, FnGameNetworkingMessagesSessionFailed )
		#endif
			default:
				AssertMsg1( false, "Unknown callback type %d!", x.nCallback );
		}

		#undef DISPATCH_CALLBACK
	}
}

void CGameNetworkingSockets::InternalQueueCallback( int nCallback, int cbCallback, const void *pvCallback, void *fnRegisteredFunctionPtr )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	if ( !fnRegisteredFunctionPtr )
		return;
	if ( cbCallback > sizeof( ((QueuedCallback*)0)->data ) )
	{
		AssertMsg( false, "Callback doesn't fit!" );
		return;
	}
	AssertMsg( len( m_vecPendingCallbacks ) < 100, "Callbacks backing up and not being checked.  Need to check them more frequently!" );

	m_mutexPendingCallbacks.lock();
	QueuedCallback &q = *push_back_get_ptr( m_vecPendingCallbacks );
	q.nCallback = nCallback;
	q.fnCallback = fnRegisteredFunctionPtr;
	memcpy( q.data, pvCallback, cbCallback );
	m_mutexPendingCallbacks.unlock();
}

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkingUtils
//
/////////////////////////////////////////////////////////////////////////////

CGameNetworkingUtils::~CGameNetworkingUtils() {}

GameNetworkingMessage_t *CGameNetworkingUtils::AllocateMessage( int cbAllocateBuffer )
{
	return CGameNetworkingMessage::New( cbAllocateBuffer );
}

GameNetworkingMicroseconds CGameNetworkingUtils::GetLocalTimestamp()
{
	return GameNetworkingSockets_GetLocalTimestamp();
}

void CGameNetworkingUtils::SetDebugOutputFunction( EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc )
{
	GameNetworkingSockets_SetDebugOutputFunction( eDetailLevel, pfnFunc );
}


template<typename T>
static ConfigValue<T> *GetConnectionVar( const GlobalConfigValueEntry *pEntry, ConnectionConfig *pConnectionConfig )
{
	Assert( pEntry->m_eScope == k_EGameNetworkingConfig_Connection );
	intptr_t ptr = intptr_t( pConnectionConfig );
	return (ConfigValue<T> *)( ptr + pEntry->m_cbOffsetOf );
}

template<typename T>
static ConfigValue<T> *EvaluateScopeConfigValue( GlobalConfigValueEntry *pEntry,
	EGameNetworkingConfigScope eScopeType,
	intptr_t scopeObj,
	ConnectionScopeLock &connectionLock // Lock this, if it's a connection
)
{
	switch ( eScopeType )
	{
		case k_EGameNetworkingConfig_Global:
		{
			auto *pGlobalVal = static_cast< GlobalConfigValueBase<T> * >( pEntry );
			return &pGlobalVal->m_value;
		}

		case k_EGameNetworkingConfig_SocketsInterface:
		{
			CGameNetworkingSockets *pInterface = (CGameNetworkingSockets *)scopeObj;
			if ( pEntry->m_eScope == k_EGameNetworkingConfig_Connection )
			{
				return GetConnectionVar<T>( pEntry, &pInterface->m_connectionConfig );
			}
			break;
		}

		case k_EGameNetworkingConfig_ListenSocket:
		{
			CGameNetworkListenSocketBase *pSock = GetListenSocketByHandle( HSteamListenSocket( scopeObj ) );
			if ( pSock )
			{
				if ( pEntry->m_eScope == k_EGameNetworkingConfig_Connection )
				{
					return GetConnectionVar<T>( pEntry, &pSock->m_connectionConfig );
				}
			}
			break;
		}

		case k_EGameNetworkingConfig_Connection:
		{
			// NOTE: Not using GetConnectionByHandleForAPI here.  In a few places in the code,
			// we need to be able to set config options for connections that are being created.
			// Really, we ought to plumb through these calls to an internal interface, so that
			// we would know that they should be given access.  Right now they are coming in
			// the "front door".  So this means if the app tries to set a config option on a
			// connection that technically no longer exists, we will actually allow that, when
			// we probably should fail the call.
			CGameNetworkConnectionBase *pConn = GetConnectionByHandle( HGameNetConnection( scopeObj ), connectionLock );
			if ( pConn )
			{
				if ( pEntry->m_eScope == k_EGameNetworkingConfig_Connection )
				{
					return GetConnectionVar<T>( pEntry, &pConn->m_connectionConfig );
				}
			}
			break;
		}

	}

	// Bad scope argument
	return nullptr;
}

static bool AssignConfigValueTyped( int32 *pVal, EGameNetworkingConfigDataType eDataType, const void *pArg )
{
	switch ( eDataType )
	{
		case k_EGameNetworkingConfig_Int32:
			*pVal = *(int32*)pArg;
			break;

		case k_EGameNetworkingConfig_Int64:
		{
			int64 arg = *(int64*)pArg;
			if ( (int32)arg != arg )
				return false; // Cannot truncate!
			*pVal = *(int32*)arg;
			break;
		}

		case k_EGameNetworkingConfig_Float:
			*pVal = (int32)floor( *(float*)pArg + .5f );
			break;

		case k_EGameNetworkingConfig_String:
		{
			int x;
			if ( sscanf( (const char *)pArg, "%d", &x ) != 1 )
				return false;
			*pVal = x;
			break;
		}

		default:
			return false;
	}

	return true;
}

static bool AssignConfigValueTyped( int64 *pVal, EGameNetworkingConfigDataType eDataType, const void *pArg )
{
	switch ( eDataType )
	{
		case k_EGameNetworkingConfig_Int32:
			*pVal = *(int32*)pArg;
			break;

		case k_EGameNetworkingConfig_Int64:
		{
			*pVal = *(int64*)pArg;
			break;
		}

		case k_EGameNetworkingConfig_Float:
			*pVal = (int64)floor( *(float*)pArg + .5f );
			break;

		case k_EGameNetworkingConfig_String:
		{
			long long x;
			if ( sscanf( (const char *)pArg, "%lld", &x ) != 1 )
				return false;
			*pVal = (int64)x;
			break;
		}

		default:
			return false;
	}

	return true;
}

static bool AssignConfigValueTyped( float *pVal, EGameNetworkingConfigDataType eDataType, const void *pArg )
{
	switch ( eDataType )
	{
		case k_EGameNetworkingConfig_Int32:
			*pVal = (float)( *(int32*)pArg );
			break;

		case k_EGameNetworkingConfig_Int64:
		{
			*pVal = (float)( *(int64*)pArg );
			break;
		}

		case k_EGameNetworkingConfig_Float:
			*pVal = *(float*)pArg;
			break;

		case k_EGameNetworkingConfig_String:
		{
			float x;
			if ( sscanf( (const char *)pArg, "%f", &x ) != 1 )
				return false;
			*pVal = x;
			break;
		}

		default:
			return false;
	}

	return true;
}

static bool AssignConfigValueTyped( std::string *pVal, EGameNetworkingConfigDataType eDataType, const void *pArg )
{
	char temp[64];

	switch ( eDataType )
	{
		case k_EGameNetworkingConfig_Int32:
			V_sprintf_safe( temp, "%d", *(int32*)pArg );
			*pVal = temp;
			break;

		case k_EGameNetworkingConfig_Int64:
			V_sprintf_safe( temp, "%lld", (long long)*(int64*)pArg );
			*pVal = temp;
			break;

		case k_EGameNetworkingConfig_Float:
			V_sprintf_safe( temp, "%g", *(float*)pArg );
			*pVal = temp;
			break;

		case k_EGameNetworkingConfig_String:
			*pVal = (const char *)pArg;
			break;

		default:
			return false;
	}

	return true;
}

static bool AssignConfigValueTyped( void **pVal, EGameNetworkingConfigDataType eDataType, const void *pArg )
{
	switch ( eDataType )
	{
		case k_EGameNetworkingConfig_Ptr:
			*pVal = *(void **)pArg;
			break;

		default:
			return false;
	}

	return true;
}

template<typename T>
bool SetConfigValueTyped(
	GlobalConfigValueEntry *pEntry,
	EGameNetworkingConfigScope eScopeType,
	intptr_t scopeObj,
	EGameNetworkingConfigDataType eDataType,
	const void *pArg
) {
	ConnectionScopeLock connectionLock;
	ConfigValue<T> *pVal = EvaluateScopeConfigValue<T>( pEntry, eScopeType, scopeObj, connectionLock );
	if ( !pVal )
		return false;

	// Locked values cannot be changed
	if ( pVal->IsLocked() )
		return false;

	// Clearing the value?
	if ( pArg == nullptr )
	{
		if ( eScopeType == k_EGameNetworkingConfig_Global )
		{
			auto *pGlobal = (typename GlobalConfigValueBase<T>::Value *)( pVal );
			Assert( pGlobal->m_pInherit == nullptr );
			Assert( pGlobal->IsSet() );
			pGlobal->m_data = pGlobal->m_defaultValue;
		}
		else if ( eScopeType == k_EGameNetworkingConfig_Connection && pEntry->m_eValue == k_EGameNetworkingConfig_ConnectionUserData )
		{
			// Once this is set, we cannot clear it or inherit it.
			SpewError( "Cannot clear connection user data\n" );
			return false;
		}
		else
		{
			Assert( pVal->m_pInherit );
			pVal->m_eState = ConfigValueBase::kENotSet;
		}
		return true;
	}

	// Call type-specific method to set it
	if ( !AssignConfigValueTyped( &pVal->m_data, eDataType, pArg ) )
		return false;

	// Mark it as set
	pVal->m_eState = ConfigValueBase::kESet;

	// Apply limits
	pEntry->Clamp<T>( pVal->m_data );

	// OK
	return true;
}

template<typename T>
EGameNetworkingGetConfigValueResult ReturnConfigValueTyped( const T &data, void *pData, size_t *cbData )
{
	EGameNetworkingGetConfigValueResult eResult;
	if ( !pData || *cbData < sizeof(T) )
	{
		eResult = k_EGameNetworkingGetConfigValue_BufferTooSmall;
	}
	else
	{
		*(T*)pData = data;
		eResult = k_EGameNetworkingGetConfigValue_OK;
	}
	*cbData = sizeof(T);
	return eResult;
}

template<>
EGameNetworkingGetConfigValueResult ReturnConfigValueTyped<std::string>( const std::string &data, void *pData, size_t *cbData )
{
	size_t l = data.length() + 1;
	EGameNetworkingGetConfigValueResult eResult;
	if ( !pData || *cbData < l )
	{
		eResult = k_EGameNetworkingGetConfigValue_BufferTooSmall;
	}
	else
	{
		memcpy( pData, data.c_str(), l );
		eResult = k_EGameNetworkingGetConfigValue_OK;
	}
	*cbData = l;
	return eResult;
}

template<typename T>
EGameNetworkingGetConfigValueResult GetConfigValueTyped(
	GlobalConfigValueEntry *pEntry,
	EGameNetworkingConfigScope eScopeType,
	intptr_t scopeObj,
	void *pResult, size_t *cbResult
) {
	ConnectionScopeLock connectionLock;
	ConfigValue<T> *pVal = EvaluateScopeConfigValue<T>( pEntry, eScopeType, scopeObj, connectionLock );
	if ( !pVal )
	{
		*cbResult = 0;
		return k_EGameNetworkingGetConfigValue_BadScopeObj;
	}

	// Remember if it was set at this level
	bool bValWasSet = pVal->IsSet();

	// Find the place where the actual value comes from
	while ( !pVal->IsSet() )
	{
		Assert( pVal->m_pInherit );
		pVal = static_cast<ConfigValue<T> *>( pVal->m_pInherit );
	}

	// Call type-specific method to return it
	EGameNetworkingGetConfigValueResult eResult = ReturnConfigValueTyped( pVal->m_data, pResult, cbResult );
	if ( eResult == k_EGameNetworkingGetConfigValue_OK && !bValWasSet )
		eResult = k_EGameNetworkingGetConfigValue_OKInherited;
	return eResult;
}

bool CGameNetworkingUtils::SetConfigValue( EGameNetworkingConfigValue eValue,
	EGameNetworkingConfigScope eScopeType, intptr_t scopeObj,
	EGameNetworkingConfigDataType eDataType, const void *pValue )
{

	// Check for special values
	switch ( eValue )
	{
		case k_EGameNetworkingConfig_MTU_DataSize:
			SpewWarning( "MTU_DataSize is readonly" );
			return false;

		case k_EGameNetworkingConfig_ConnectionUserData:
		{

			// We only need special handling when modifying a connection
			if ( eScopeType != k_EGameNetworkingConfig_Connection )
				break;

			// Process the user argument, maybe performing type conversion
			int64 newData;
			if ( !AssignConfigValueTyped( &newData, eDataType, pValue ) )
				return false;

			// Lookup the connection
			ConnectionScopeLock connectionLock;
			CGameNetworkConnectionBase *pConn = GetConnectionByHandle( HGameNetConnection( scopeObj ), connectionLock );
			if ( !pConn )
				return false;

			// Set the data, possibly fixing up existing queued messages, etc
			pConn->SetUserData( pConn->m_connectionConfig.m_ConnectionUserData.m_data );
			return true;
		}

	}

	GlobalConfigValueEntry *pEntry = FindConfigValueEntry( eValue );
	if ( pEntry == nullptr )
		return false;

	GameNetworkingGlobalLock scopeLock( "SetConfigValue" );

	switch ( pEntry->m_eDataType )
	{
		case k_EGameNetworkingConfig_Int32: return SetConfigValueTyped<int32>( pEntry, eScopeType, scopeObj, eDataType, pValue );
		case k_EGameNetworkingConfig_Int64: return SetConfigValueTyped<int64>( pEntry, eScopeType, scopeObj, eDataType, pValue );
		case k_EGameNetworkingConfig_Float: return SetConfigValueTyped<float>( pEntry, eScopeType, scopeObj, eDataType, pValue );
		case k_EGameNetworkingConfig_String: return SetConfigValueTyped<std::string>( pEntry, eScopeType, scopeObj, eDataType, pValue );
		case k_EGameNetworkingConfig_Ptr: return SetConfigValueTyped<void *>( pEntry, eScopeType, scopeObj, eDataType, pValue );
	}

	Assert( false );
	return false;
}

EGameNetworkingGetConfigValueResult CGameNetworkingUtils::GetConfigValue(
	EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType,
	intptr_t scopeObj, EGameNetworkingConfigDataType *pOutDataType,
	void *pResult, size_t *cbResult )
{
	// Take the global lock.
	GameNetworkingGlobalLock scopeLock( "GetConfigValue" );

	if ( eValue == k_EGameNetworkingConfig_MTU_DataSize )
	{
		int32 MTU_packetsize;
		size_t cbMTU_packetsize = sizeof(MTU_packetsize);
		EGameNetworkingGetConfigValueResult rFetch = GetConfigValueTyped<int32>( &g_ConfigDefault_MTU_PacketSize, eScopeType, scopeObj, &MTU_packetsize, &cbMTU_packetsize );
		if ( rFetch < 0 )
			return rFetch;

		int32 MTU_DataSize = std::max( 0, MTU_packetsize - k_cbGameNetworkingSocketsNoFragmentHeaderReserve );
		EGameNetworkingGetConfigValueResult rStore = ReturnConfigValueTyped<int32>( MTU_DataSize, pResult, cbResult );
		if ( rStore != k_EGameNetworkingGetConfigValue_OK )
			return rStore;
		return rFetch;
	}

	GlobalConfigValueEntry *pEntry = FindConfigValueEntry( eValue );
	if ( pEntry == nullptr )
		return k_EGameNetworkingGetConfigValue_BadValue;

	if ( pOutDataType )
		*pOutDataType = pEntry->m_eDataType;

	switch ( pEntry->m_eDataType )
	{
		case k_EGameNetworkingConfig_Int32: return GetConfigValueTyped<int32>( pEntry, eScopeType, scopeObj, pResult, cbResult );
		case k_EGameNetworkingConfig_Int64: return GetConfigValueTyped<int64>( pEntry, eScopeType, scopeObj, pResult, cbResult );
		case k_EGameNetworkingConfig_Float: return GetConfigValueTyped<float>( pEntry, eScopeType, scopeObj, pResult, cbResult );
		case k_EGameNetworkingConfig_String: return GetConfigValueTyped<std::string>( pEntry, eScopeType, scopeObj, pResult, cbResult );
		case k_EGameNetworkingConfig_Ptr: return GetConfigValueTyped<void *>( pEntry, eScopeType, scopeObj, pResult, cbResult );
	}

	Assert( false ); // FIXME
	return k_EGameNetworkingGetConfigValue_BadValue;
}

static bool BEnumerateConfigValue( const GlobalConfigValueEntry *pVal )
{
	if ( pVal->m_eDataType == k_EGameNetworkingConfig_Ptr )
		return false;

	switch  ( pVal->m_eValue )
	{
		// Never enumerate these
		case k_EGameNetworkingConfig_SymmetricConnect:
		case k_EGameNetworkingConfig_LocalVirtualPort:
		case k_EGameNetworkingConfig_ConnectionUserData:
			return false;

		// Dev var?
		case k_EGameNetworkingConfig_IP_AllowWithoutAuth:
		case k_EGameNetworkingConfig_Unencrypted:
		case k_EGameNetworkingConfig_EnumerateDevVars:
		case k_EGameNetworkingConfig_SDRClient_FakeClusterPing:
			return g_Config_EnumerateDevVars.Get();
	}

	return true;
}

bool CGameNetworkingUtils::GetConfigValueInfo( EGameNetworkingConfigValue eValue,
	const char **pOutName, EGameNetworkingConfigDataType *pOutDataType,
	EGameNetworkingConfigScope *pOutScope, EGameNetworkingConfigValue *pOutNextValue )
{
	const GlobalConfigValueEntry *pVal = FindConfigValueEntry( eValue );
	if ( pVal == nullptr )
		return false;

	if ( pOutName )
		*pOutName = pVal->m_pszName;
	if ( pOutDataType )
		*pOutDataType = pVal->m_eDataType;
	if ( pOutScope )
		*pOutScope = pVal->m_eScope;

	if ( pOutNextValue )
	{
		const GlobalConfigValueEntry *pNext = pVal;
		for (;;)
		{
			pNext = pNext->m_pNextEntry;
			if ( !pNext )
			{
				*pOutNextValue = k_EGameNetworkingConfig_Invalid;
				break;
			}
			if ( BEnumerateConfigValue( pNext ) )
			{
				*pOutNextValue = pNext->m_eValue;
				break;
			}
		};
	}

	return true;
}

EGameNetworkingConfigValue CGameNetworkingUtils::GetFirstConfigValue()
{
	EnsureConfigValueTableInitted();
	Assert( BEnumerateConfigValue( s_vecConfigValueTable[0] ) );
	return s_vecConfigValueTable[0]->m_eValue;
}


void CGameNetworkingUtils::GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr &addr, char *buf, size_t cbBuf, bool bWithPort )
{
	::GameNetworkingIPAddr_ToString( &addr, buf, cbBuf, bWithPort );
}

bool CGameNetworkingUtils::GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr *pAddr, const char *pszStr )
{
	return ::GameNetworkingIPAddr_ParseString( pAddr, pszStr );
}

void CGameNetworkingUtils::GameNetworkingIdentity_ToString( const GameNetworkingIdentity &identity, char *buf, size_t cbBuf )
{
	return ::GameNetworkingIdentity_ToString( &identity, buf, cbBuf );
}

bool CGameNetworkingUtils::GameNetworkingIdentity_ParseString( GameNetworkingIdentity *pIdentity, const char *pszStr )
{
	return ::GameNetworkingIdentity_ParseString( pIdentity, sizeof(GameNetworkingIdentity), pszStr );
}

AppId_t CGameNetworkingUtils::GetAppID()
{
	return m_nAppID;
}

void CGameNetworkingUtils::TEST_ResetSelf()
{
	m_nAppID = 0;
}

time_t CGameNetworkingUtils::GetTimeSecure()
{
	// Trusting local user's clock!
	return time(nullptr);
}

const char *CGameNetworkingUtils::GetBuildString()
{
	#if defined( STEAMNETWORKINGSOCKETS_OPENSOURCE )
		return "opensource " __DATE__ " " __TIME__;
	#elif defined( STEAMNETWORKINGSOCKETS_PARTNER )
		return "partner " __DATE__ " " __TIME__;
	#elif defined( STEAMNETWORKINGSOCKETS_STANDALONELIB )
		return "lib " __DATE__ " " __TIME__;
	#elif defined( STEAMNETWORKINGSOCKETS_STEAMCLIENT )
		return "game "
		#ifdef BRANCH_MAIN
			"(main) "
		#elif !defined( BRANCH_REL_CLIENT )
			"(branch?) "
		#endif
		__DATE__ " " __TIME__;
	#elif defined( STEAMNETWORKINGSOCKETS_STREAMINGCLIENT )
		return "stream "
		#ifdef BRANCH_MAIN
			"(main) "
		#elif !defined( BRANCH_REL_CLIENT )
			"(branch?) "
		#endif
		__DATE__ " " __TIME__;
	#else
		#error "Huh?"
	#endif
}

const char *CGameNetworkingUtils::GetPlatformString()
{
	#if defined( NN_NINTENDO_SDK )
		return "nswitch";
	#elif defined( _GAMECORE )
		// Is this right?  This might actually require a system call.
		return "xboxx";
	#elif defined( _STADIA )
		// Not sure if this works.
		return "stadia";
	#elif defined( _XBOX_ONE )
		return "xbone";
	#elif defined( _PS4 )
		return "ps4";
	#elif defined( _PS5 )
		return "ps5";
	#elif defined( TVOS ) || defined( __TVOS__ )
		return "tvos";
	#elif defined( __APPLE__ )
		#if TARGET_OS_TV
			return "tvos";
		#elif TARGET_OS_IPHONE
			return "ios";
		#else
			return "osx";
		#endif
	#elif defined( OSX )
		return "osx";
	#elif defined( ANDROID ) || defined( __ANDROID__ )
		return "android";
	#elif defined( _WINDOWS )
		return "windows";
	#elif defined( LINUX ) || defined( __LINUX__ ) || defined(linux) || defined(__linux) || defined(__linux__)
		return "linux";
	#elif defined( FREEBSD ) || defined( __FreeBSD__ )
		return "freebsd";
	#else
		#error "Unknown platform"
	#endif
}

} // namespace GameNetworkingSocketsLib
using namespace GameNetworkingSocketsLib;

/////////////////////////////////////////////////////////////////////////////
//
// Global API interface
//
/////////////////////////////////////////////////////////////////////////////

#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE

static CGameNetworkingSockets *s_pGameNetworkingSockets = nullptr;

STEAMNETWORKINGSOCKETS_INTERFACE bool GameNetworkingSockets_Init( const GameNetworkingIdentity *pIdentity, GameNetworkingErrMsg &errMsg )
{
	GameNetworkingGlobalLock lock( "GameNetworkingSockets_Init" );

	// Already initted?
	if ( s_pGameNetworkingSockets )
	{
		AssertMsg( false, "GameNetworkingSockets_init called multiple times?" );
		return true;
	}

	// Init basic functionality
	CGameNetworkingSockets *pGameNetworkingSockets = new CGameNetworkingSockets( ( CGameNetworkingUtils *)GameNetworkingUtils() );
	if ( !pGameNetworkingSockets->BInitGameNetworkingSockets( pIdentity, errMsg ) )
	{
		pGameNetworkingSockets->Destroy();
		return false;
	}

	s_pGameNetworkingSockets = pGameNetworkingSockets;
	return true;
}

STEAMNETWORKINGSOCKETS_INTERFACE void GameNetworkingSockets_Kill()
{
	GameNetworkingGlobalLock lock( "GameNetworkingSockets_Kill" );
	if ( s_pGameNetworkingSockets )
	{
		s_pGameNetworkingSockets->Destroy();
		s_pGameNetworkingSockets = nullptr;
	}
}

STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingSockets *GameNetworkingSockets_LibV9()
{
	return s_pGameNetworkingSockets;
}

STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingUtils *GameNetworkingUtils_LibV3()
{
	static CGameNetworkingUtils s_utils;
	return &s_utils;
}

#endif

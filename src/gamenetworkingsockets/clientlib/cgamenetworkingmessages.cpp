//====== Copyright Valve Corporation, All rights reserved. ====================

#include "cgamenetworkingmessages.h"
#include "cgamenetworkingsockets.h"
#include "gamenetworkingsockets_p2p.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Make sure we're enabled
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES

#pragma pack(push,1)
struct P2PMessageHeader
{
	uint8 m_nFlags;
	int m_nToChannel;
};
#pragma pack(pop)
COMPILE_TIME_ASSERT( sizeof(P2PMessageHeader) == 5 );

// FIXME TODO:
// * Need to clear P2P error when we start connecting or get a successful result
// * When we get P2P error callback from game, need to flow that back up through to the session
// * Handle race condition when we try to send a message right as the connection is timing out
// * Nuke interface when higher level Kill calls are made
// * Only do kludge to always send early messages as reliable on old code, not new code.

// Put everything in a namespace, so we don't violate the one definition rule
namespace GameNetworkingSocketsLib {

const GameNetworkingMicroseconds k_usecGameNetworkingP2PSessionIdleTimeout = 3*60*k_nMillion;
const int k_EGameNetConnectionEnd_P2P_SessionClosed = k_EGameNetConnectionEnd_App_Min + 1;
const int k_EGameNetConnectionEnd_P2P_SessionIdleTimeout = k_EGameNetConnectionEnd_App_Min + 2;

// These tables are protected by global lock
static CUtlHashMap<HGameNetConnection,GameNetworkingMessagesSession*,std::equal_to<HGameNetConnection>,std::hash<HGameNetConnection>> g_mapSessionsByConnection;
static CUtlHashMap<HSteamListenSocket,CGameNetworkingMessages*,std::equal_to<HSteamListenSocket>,std::hash<HSteamListenSocket>> g_mapMessagesInterfaceByListenSocket;

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkingSockets
//
/////////////////////////////////////////////////////////////////////////////

CGameNetworkingMessages *CGameNetworkingSockets::GetGameNetworkingMessages()
{
	// Already exist?
	if ( !m_pGameNetworkingMessages )
	{
		GameNetworkingGlobalLock scopeLock;
		GameNetworkingGlobalLock::SetLongLockWarningThresholdMS( "CreateGameNetworkingMessages", 10 );
		m_pGameNetworkingMessages = new CGameNetworkingMessages( *this );
		if ( !m_pGameNetworkingMessages->BInit() )
		{
			// NOTE: We re gonna keep trying to do this and failing repeatedly.
			delete m_pGameNetworkingMessages;
			m_pGameNetworkingMessages = nullptr;
		}
	}
	return m_pGameNetworkingMessages;
}

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkingMessages
//
/////////////////////////////////////////////////////////////////////////////

CGameNetworkingMessages::Channel::Channel()
{
	m_queueRecvMessages.m_pRequiredLock = &g_lockAllRecvMessageQueues;
		
}

CGameNetworkingMessages::Channel::~Channel()
{
	ShortDurationScopeLock scopeLock( g_lockAllRecvMessageQueues );

	// Should be empty!
	Assert( m_queueRecvMessages.empty() );

	// But in case not
	m_queueRecvMessages.PurgeMessages();
}

CGameNetworkingMessages::CGameNetworkingMessages( CGameNetworkingSockets &gameNetworkingSockets )
: m_gameNetworkingSockets( gameNetworkingSockets )
{
}

bool CGameNetworkingMessages::BInit()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	// Create listen socket
	{
		GameNetworkingConfigValue_t opt[2];
		opt[0].SetPtr( k_EGameNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ConnectionStatusChangedCallback );
		opt[1].SetInt32( k_EGameNetworkingConfig_SymmetricConnect, 1 );
		m_pListenSocket = m_gameNetworkingSockets.InternalCreateListenSocketP2P( k_nVirtualPort_Messages, 2, opt );
		if ( !m_pListenSocket )
			return false;
	}

	// Add us to map by listen socket
	Assert( !g_mapMessagesInterfaceByListenSocket.HasElement( m_pListenSocket->m_hListenSocketSelf ) );
	g_mapMessagesInterfaceByListenSocket.InsertOrReplace( m_pListenSocket->m_hListenSocketSelf, this );

	// Create poll group
	PollGroupScopeLock pollGroupLock;
	m_pPollGroup = m_gameNetworkingSockets.InternalCreatePollGroup( pollGroupLock );
	if ( !m_pPollGroup )
	{
		AssertMsg( false, "Failed to create/find poll group" );
		return false;
	}

	return true;
}

void CGameNetworkingMessages::FreeResources()
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CGameNetworkingMessages::FreeResources" );

	// Destroy all of our sessions.  This will detach all of our connections
	FOR_EACH_HASHMAP( m_mapSessions, i )
	{
		DestroySession( m_mapSessions.Key(i) );
	}
	Assert( m_mapSessions.Count() == 0 );
	m_mapSessions.Purge();
	m_mapChannels.PurgeAndDeleteElements();

	// Destroy poll group, if any
	delete m_pPollGroup;
	m_pPollGroup = nullptr;

	// Destroy listen socket, if any
	if ( m_pListenSocket )
	{

		// Remove us from the map
		int idx = g_mapMessagesInterfaceByListenSocket.Find( m_pListenSocket->m_hListenSocketSelf );
		if ( idx == g_mapMessagesInterfaceByListenSocket.InvalidIndex() )
		{
			Assert( false );
		}
		else
		{
			Assert( g_mapMessagesInterfaceByListenSocket[idx] == this );
			g_mapMessagesInterfaceByListenSocket[idx] = nullptr; // Just for grins
			g_mapMessagesInterfaceByListenSocket.RemoveAt( idx );
		}

		m_pListenSocket->Destroy();
		m_pListenSocket = nullptr;
	}

	int idx = m_gameNetworkingSockets.m_mapListenSocketsByVirtualPort.Find( k_nVirtualPort_Messages );
	if ( idx != m_gameNetworkingSockets.m_mapListenSocketsByVirtualPort.InvalidIndex() )
	{
		DbgVerify( m_gameNetworkingSockets.CloseListenSocket( m_gameNetworkingSockets.m_mapListenSocketsByVirtualPort[ idx ]->m_hListenSocketSelf ) );
	}
	Assert( !m_gameNetworkingSockets.m_mapListenSocketsByVirtualPort.HasElement( k_nVirtualPort_Messages ) );
}


CGameNetworkingMessages::~CGameNetworkingMessages()
{
	GameNetworkingGlobalLock scopeLock;

	FreeResources();

	// make sure our parent knows we have been destroyed
	if ( m_gameNetworkingSockets.m_pGameNetworkingMessages == this )
	{
		m_gameNetworkingSockets.m_pGameNetworkingMessages = nullptr;
	}
	else
	{
		// We should never create more than one messages interface for any given sockets interface!
		Assert( m_gameNetworkingSockets.m_pGameNetworkingMessages == nullptr );
	}
}

void CGameNetworkingMessages::ConnectionStatusChangedCallback( GameNetConnectionStatusChangedCallback_t *pInfo )
{
	// These callbacks should happen synchronously, while we have the lock
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CGameNetworkingMessages::ConnectionStatusChangedCallback" );

	// New connection?
	if ( pInfo->m_eOldState == k_EGameNetworkingConnectionState_None )
	{

		// New connections should only ever transition into to the "connecting" state
		if ( pInfo->m_info.m_eState != k_EGameNetworkingConnectionState_Connecting )
		{
			AssertMsg( false, "Unexpected state transition from 'none' to %d", pInfo->m_info.m_eState );
			return;
		}

		// Are we initiating this?
		if ( pInfo->m_info.m_hListenSocket == k_HSteamListenSocket_Invalid )
			return; // ignore

		// New incoming connection.
		int h = g_mapMessagesInterfaceByListenSocket.Find( pInfo->m_info.m_hListenSocket );
		if ( h == g_mapSessionsByConnection.InvalidIndex() )
		{
			AssertMsg( false, "ConnectionStatusChangedCallback, but listen socket not found?" );

			// FIXME - if we hit this bug, we leak the connection.  Should we try to clean up better?
			return;
		}
		ConnectionScopeLock connectionLock;
		CGameNetworkConnectionBase *pConn = GetConnectionByHandle( pInfo->m_hConn, connectionLock );
		if ( !pConn )
		{
			AssertMsg( false, "Can't find connection by handle?" );
			return;
		}
		Assert( pConn->GetState() == k_EGameNetworkingConnectionState_Connecting );
		g_mapMessagesInterfaceByListenSocket[h]->NewConnection( pConn );
		return;
	}

	// Change to a known connection with a session?
	int h = g_mapSessionsByConnection.Find( pInfo->m_hConn );
	if ( h != g_mapSessionsByConnection.InvalidIndex() )
	{
		g_mapSessionsByConnection[h]->ConnectionStateChanged( pInfo );
		return;
	}
}

EResult CGameNetworkingMessages::SendMessageToUser( const GameNetworkingIdentity &identityRemote, const void *pubData, uint32 cubData, int nSendFlags, int nRemoteChannel )
{
	// FIXME GameNetworkingIdentity
	if ( identityRemote.GetSteamID64() == 0 )
	{
		AssertMsg1( false, "Identity %s isn't valid for Messages sessions.  (Only SteamIDs currently supported).", GameNetworkingIdentityRender( identityRemote ).c_str() );
		return k_EResultInvalidSteamID;
	}
	if ( !IsValidSteamIDForIdentity( identityRemote.GetSteamID() ) )
	{
		AssertMsg1( false, "%s isn't valid SteamID for identity.", identityRemote.GetSteamID().Render() );
		return k_EResultInvalidSteamID;
	}

	GameNetworkingGlobalLock scopeLock( "SendMessageToUser" ); // NOTE - Messages sessions are protected by the global lock.  We have not optimized for more granular locking of the Messages interface
	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSess = FindOrCreateSession( identityRemote, connectionLock );
	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	// Check on connection if needed
	pSess->CheckConnection( usecNow );

	CGameNetworkConnectionBase *pConn = pSess->m_pConnection;
	if ( pConn )
	{

		// Implicit accept?
		if ( pConn->m_bConnectionInitiatedRemotely && pConn->GetState() == k_EGameNetworkingConnectionState_Connecting )
		{
			SpewVerbose( "Messages session %s: Implicitly accepted connection %s via SendMessageToUser\n", GameNetworkingIdentityRender( identityRemote ).c_str(), pConn->GetDescription() );
			pConn->APIAcceptConnection();
			pSess->UpdateConnectionInfo();
		}
	}
	else
	{

		// No active connection.
		// Did the previous one fail?
		GameNetConnectionInfo_t &info = pSess->m_lastConnectionInfo;
		if ( info.m_eState != k_EGameNetworkingConnectionState_None )
		{
			if ( !( nSendFlags & k_nGameNetworkingSend_AutoRestartBrokenSession ) )
			{
				SpewVerbose( "Previous messages connection %s broken (%d, %s), rejecting SendMessageToUser\n",
					info.m_szConnectionDescription, info.m_eEndReason, info.m_szEndDebug );
				return k_EResultConnectFailed;
			}

			SpewVerbose( "Previous messages connection %s broken (%d, %s), restarting session as per AutoRestartBrokenSession\n",
				info.m_szConnectionDescription, info.m_eEndReason, info.m_szEndDebug );
			memset( &info, 0, sizeof(info) );
			memset( &pSess->m_lastQuickStatus, 0, sizeof(pSess->m_lastQuickStatus) );
		}

		// Try to create one
		GameNetworkingConfigValue_t opt[2];
		opt[0].SetPtr( k_EGameNetworkingConfig_Callback_ConnectionStatusChanged, (void*)ConnectionStatusChangedCallback );
		opt[1].SetInt32( k_EGameNetworkingConfig_SymmetricConnect, 1 );
		pConn = m_gameNetworkingSockets.InternalConnectP2PDefaultSignaling( identityRemote, k_nVirtualPort_Messages, 2, opt, connectionLock );
		if ( !pConn )
		{
			AssertMsg( false, "Failed to create connection to '%s' for new messages session", GameNetworkingIdentityRender( identityRemote ).c_str() );
			return k_EResultFail;
		}

		SpewVerbose( "[%s] Created connection for messages session\n", pConn->GetDescription() );
		pSess->LinkConnection( pConn );
	}

	// KLUDGE Old P2P always sent messages that had to be queued reliably!
	// (It had to do with better buffering or something.)  If we change this,
	// we are almost certainly going to break some games that depend on it.
	// Yes, this is kind of crazy, we should try to scope it tighter.
	if ( pConn->GetState() != k_EGameNetworkingConnectionState_Connected )
		nSendFlags = k_nGameNetworkingSend_Reliable;

	// Allocate a message, and put our header in front.
	int cbSend = cubData + sizeof(P2PMessageHeader);
	CGameNetworkingMessage *pMsg = (CGameNetworkingMessage *)m_gameNetworkingSockets.m_pGameNetworkingUtils->AllocateMessage( cbSend );
	if ( !pMsg )
	{
		pSess->m_pConnection->ConnectionState_ProblemDetectedLocally( k_EGameNetConnectionEnd_AppException_Generic, "Failed to allocate message" );
		return k_EResultFail;
	}
	pMsg->m_nFlags = nSendFlags;

	P2PMessageHeader *hdr = static_cast<P2PMessageHeader *>( pMsg->m_pData );
	hdr->m_nFlags = 1;
	hdr->m_nToChannel = LittleDWord( nRemoteChannel );
	memcpy( hdr+1, pubData, cubData );

	// Reset idle timeout, schedule a wakeup call
	pSess->MarkUsed( usecNow );

	// Send it
	int64 nMsgNumberOrResult = pConn->_APISendMessageToConnection( pMsg, usecNow, nullptr );
	if ( nMsgNumberOrResult > 0 )
		return k_EResultOK;
	return EResult( -nMsgNumberOrResult );
}

int CGameNetworkingMessages::ReceiveMessagesOnChannel( int nLocalChannel, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages )
{
	GameNetworkingGlobalLock scopeLock( "ReceiveMessagesOnChannel" );

	Channel *pChan = FindOrCreateChannel( nLocalChannel );

	ShortDurationScopeLock lockMessageQueues( g_lockAllRecvMessageQueues );

	// Pull out all messages from the poll group into per-channel queues
	if ( m_pPollGroup )
	{
		for (;;)
		{
			CGameNetworkingMessage *pMsg = m_pPollGroup->m_queueRecvMessages.m_pFirst;
			if ( !pMsg )
				break;
			pMsg->Unlink();

			int idxSession = g_mapSessionsByConnection.Find( pMsg->m_conn );
			if ( idxSession == g_mapSessionsByConnection.InvalidIndex() )
			{
				pMsg->Release();
				continue;
			}

			GameNetworkingMessagesSession *pSess = g_mapSessionsByConnection[ idxSession ];
			Assert( pSess->m_pConnection );
			Assert( this == &pSess->m_gameNetworkingMessagesOwner );
			pSess->ReceivedMessage( pMsg );
		}
	}

	return pChan->m_queueRecvMessages.RemoveMessages( ppOutMessages, nMaxMessages );
}

bool CGameNetworkingMessages::AcceptSessionWithUser( const GameNetworkingIdentity &identityRemote )
{
	GameNetworkingGlobalLock scopeLock( "AcceptSessionWithUser" );
	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSession = FindSession( identityRemote, connectionLock );
	if ( !pSession )
		return false;

	GameNetworkingMicroseconds usecNow = GameNetworkingSockets_GetLocalTimestamp();

	// Then there should be a connection
	CGameNetworkConnectionBase *pConn = pSession->m_pConnection;
	if ( !pConn )
		return false;
	if ( pConn->m_bConnectionInitiatedRemotely && pConn->GetState() == k_EGameNetworkingConnectionState_Connecting )
		pConn->APIAcceptConnection();
	pSession->MarkUsed( usecNow );
	return true;
}

bool CGameNetworkingMessages::CloseSessionWithUser( const GameNetworkingIdentity &identityRemote )
{
	GameNetworkingGlobalLock scopeLock( "CloseSessionWithUser" );
	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSession = FindSession( identityRemote, connectionLock );
	if ( !pSession )
		return false;

	pSession->CloseConnection( k_EGameNetConnectionEnd_P2P_SessionClosed, "CloseSessionWithUser" );

	DestroySession( identityRemote );
	return true;
}

bool CGameNetworkingMessages::CloseChannelWithUser( const GameNetworkingIdentity &identityRemote, int nChannel )
{
	GameNetworkingGlobalLock scopeLock( "CloseChannelWithUser" );
	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSession = FindSession( identityRemote, connectionLock );
	if ( !pSession )
		return false;

	// Did we even have that channel open with this user?
	int h = pSession->m_mapOpenChannels.Find( nChannel );
	if ( h == pSession->m_mapOpenChannels.InvalidIndex() )
		return false;
	pSession->m_mapOpenChannels.RemoveAt(h);

	// Destroy all unread messages on this channel from this user
	CGameNetworkingMessage **ppMsg = &pSession->m_queueRecvMessages.m_pFirst;
	for (;;)
	{
		CGameNetworkingMessage *pMsg = *ppMsg;
		if ( pMsg == nullptr )
			break;
		Assert( pMsg->m_identityPeer == identityRemote );
		if ( pMsg->GetChannel() == nChannel )
		{
			pMsg->Unlink();
			Assert( *ppMsg != pMsg );
			pMsg->Release();
		}
		else
		{
			ppMsg = &pMsg->m_links.m_pPrev;
		}
	}

	// No more open channels?
	if ( pSession->m_mapOpenChannels.Count() == 0 )
		CloseSessionWithUser( identityRemote );
	return true;
}

EGameNetworkingConnectionState CGameNetworkingMessages::GetSessionConnectionInfo( const GameNetworkingIdentity &identityRemote, GameNetConnectionInfo_t *pConnectionInfo, GameNetworkingQuickConnectionStatus *pQuickStatus )
{
	GameNetworkingGlobalLock scopeLock( "GetSessionConnectionInfo" );
	if ( pConnectionInfo )
		memset( pConnectionInfo, 0, sizeof(*pConnectionInfo) );
	if ( pQuickStatus )
		memset( pQuickStatus, 0, sizeof(*pQuickStatus) );

	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSess = FindSession( identityRemote, connectionLock );
	if ( pSess == nullptr )
		return k_EGameNetworkingConnectionState_None;

	pSess->UpdateConnectionInfo();

	if ( pConnectionInfo )
		*pConnectionInfo = pSess->m_lastConnectionInfo;
	if ( pQuickStatus )
		*pQuickStatus = pSess->m_lastQuickStatus;

	return pSess->m_lastConnectionInfo.m_eState;
}

GameNetworkingMessagesSession *CGameNetworkingMessages::FindSession( const GameNetworkingIdentity &identityRemote, ConnectionScopeLock &connectionLock )
{
	Assert( !connectionLock.IsLocked() );
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();
	int h = m_mapSessions.Find( identityRemote );
	if ( h == m_mapSessions.InvalidIndex() )
		return nullptr;
	GameNetworkingMessagesSession *pResult = m_mapSessions[ h ];
	Assert( pResult->m_identityRemote == identityRemote );
	if ( pResult->m_pConnection )
		connectionLock.Lock( *pResult->m_pConnection );
	return pResult;
}

GameNetworkingMessagesSession *CGameNetworkingMessages::FindOrCreateSession( const GameNetworkingIdentity &identityRemote, ConnectionScopeLock &connectionLock )
{
	GameNetworkingMessagesSession *pResult = FindSession( identityRemote, connectionLock );
	if ( !pResult )
	{
		SpewVerbose( "Messages session %s: created\n", GameNetworkingIdentityRender( identityRemote ).c_str() );
		pResult = new GameNetworkingMessagesSession( identityRemote, *this );
		m_mapSessions.Insert( identityRemote, pResult );
	}

	Assert( ( pResult->m_pConnection != nullptr ) == connectionLock.IsLocked() );

	return pResult;
}

CGameNetworkingMessages::Channel *CGameNetworkingMessages::FindOrCreateChannel( int nChannel )
{
	int h = m_mapChannels.Find( nChannel );
	if ( h != m_mapChannels.InvalidIndex() )
		return m_mapChannels[h];
	Channel *pChan = new Channel;
	m_mapChannels.Insert( nChannel, pChan );
	return pChan;
}

void CGameNetworkingMessages::DestroySession( const GameNetworkingIdentity &identityRemote )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread( "CGameNetworkingMessages::DestroySession" );
	int h = m_mapSessions.Find( identityRemote );
	if ( h == m_mapSessions.InvalidIndex() )
		return;
	GameNetworkingMessagesSession *pSess = m_mapSessions[ h ];
	Assert( pSess->m_identityRemote == identityRemote );

	// Remove from table
	m_mapSessions[ h ] = nullptr;
	m_mapSessions.RemoveAt( h );

	// Nuke session memory
	delete pSess;
}

void CGameNetworkingMessages::NewConnection( CGameNetworkConnectionBase *pConn )
{
	// All of our connections should have this flag set
	Assert( pConn->BSymmetricMode() );

	// Check if we already have a session with an open connection
	ConnectionScopeLock connectionLock;
	GameNetworkingMessagesSession *pSess = FindOrCreateSession( pConn->m_identityRemote, connectionLock );
	if ( pSess->m_pConnection )
	{
		AssertMsg( false, "Got incoming messages session connection request when we already had a connection.  This could happen legit, but we aren't handling it right now." );
		pConn->ConnectionQueueDestroy();
		return;
	}

	// Setup the association
	pSess->LinkConnection( pConn );

	// Post a callback
	GameNetworkingMessagesSessionRequest_t callback;
	callback.m_identityRemote = pConn->m_identityRemote;
	m_gameNetworkingSockets.QueueCallback( callback, g_Config_Callback_MessagesSessionRequest.Get() );
}

#ifdef DBGFLAG_VALIDATE

void CGameNetworkingMessages::Validate( CValidator &validator, const char *pchName )
{
	ValidateRecursive( m_mapSessions );
	ValidateRecursive( m_mapChannels );
}

#endif

/////////////////////////////////////////////////////////////////////////////
//
// GameNetworkingMessagesSession
//
/////////////////////////////////////////////////////////////////////////////

GameNetworkingMessagesSession::GameNetworkingMessagesSession( const GameNetworkingIdentity &identityRemote, CGameNetworkingMessages &gameNetworkingP2P )
: m_gameNetworkingMessagesOwner( gameNetworkingP2P )
, m_identityRemote( identityRemote )
{
	m_pConnection = nullptr;
	m_bConnectionStateChanged = false;
	m_bConnectionWasEverConnected = false;

	memset( &m_lastConnectionInfo, 0, sizeof(m_lastConnectionInfo) );
	memset( &m_lastQuickStatus, 0, sizeof(m_lastQuickStatus) );

	m_queueRecvMessages.m_pRequiredLock = &g_lockAllRecvMessageQueues;

	MarkUsed( GameNetworkingSockets_GetLocalTimestamp() );
}

GameNetworkingMessagesSession::~GameNetworkingMessagesSession()
{
	// Discard messages
	g_lockAllRecvMessageQueues.lock();
	m_queueRecvMessages.PurgeMessages();
	g_lockAllRecvMessageQueues.unlock();

	// If we have a connection, then nuke it now
	CloseConnection( k_EGameNetConnectionEnd_P2P_SessionClosed, "P2PSession destroyed" );
}

void GameNetworkingMessagesSession::CloseConnection( int nReason, const char *pszDebug )
{
	CGameNetworkConnectionBase *pConn = m_pConnection;
	if ( pConn )
	{
		UpdateConnectionInfo();
		UnlinkConnection();
		pConn->APICloseConnection( nReason, pszDebug, false );
	}
	ScheduleThink();
}

void GameNetworkingMessagesSession::MarkUsed( GameNetworkingMicroseconds usecNow )
{
	m_usecIdleTimeout = usecNow + k_usecGameNetworkingP2PSessionIdleTimeout;
	ScheduleThink();
}

void GameNetworkingMessagesSession::ScheduleThink()
{
	Assert( m_usecIdleTimeout > 0 ); // We should always have an idle timeout set!
	EnsureMinThinkTime( m_usecIdleTimeout );
}

void GameNetworkingMessagesSession::UpdateConnectionInfo()
{
	if ( !m_pConnection )
		return;
	if ( CollapseConnectionStateToAPIState( m_pConnection->GetState() ) == k_EGameNetworkingConnectionState_None )
		return;
	m_pConnection->ConnectionPopulateInfo( m_lastConnectionInfo );
	m_lastConnectionInfo.m_hListenSocket = k_HSteamListenSocket_Invalid; // Always clear this, we don't want users of the API to know this is a thing
	m_pConnection->APIGetQuickConnectionStatus( m_lastQuickStatus );
	if ( m_lastConnectionInfo.m_eState == k_EGameNetworkingConnectionState_Connected )
		m_bConnectionWasEverConnected = true;
}

void GameNetworkingMessagesSession::CheckConnection( GameNetworkingMicroseconds usecNow )
{
	if ( !m_pConnection || !m_bConnectionStateChanged )
		return;

	UpdateConnectionInfo();

	bool bIdle = !m_pConnection->SNP_BHasAnyBufferedRecvData()
		&& !m_pConnection->SNP_BHasAnyUnackedSentReliableData();

	// Check if the connection died
	if ( m_lastConnectionInfo.m_eState == k_EGameNetworkingConnectionState_ProblemDetectedLocally || m_lastConnectionInfo.m_eState == k_EGameNetworkingConnectionState_ClosedByPeer )
	{
		SpewVerbose( "[%s] messages session %s: %d %s\n",
			m_lastConnectionInfo.m_szConnectionDescription,
			m_lastConnectionInfo.m_eState == k_EGameNetworkingConnectionState_ProblemDetectedLocally ? "problem detected locally" : "closed by peer",
			(int)m_lastConnectionInfo.m_eEndReason, m_lastConnectionInfo.m_szEndDebug );
		if ( bIdle && m_bConnectionWasEverConnected )
		{
			SpewVerbose( "    (But connection is idle, so treating this as idle timeout on our end.)" );
			memset( &m_lastConnectionInfo, 0, sizeof(m_lastConnectionInfo) );
			memset( &m_lastQuickStatus, 0, sizeof(m_lastQuickStatus) );
		}
		else
		{
			// Post failure callback.
			SpewVerbose( "[%s] Posting GameNetworkingMessagesSessionFailed_t\n", m_lastConnectionInfo.m_szConnectionDescription );
			GameNetworkingMessagesSessionFailed_t callback;
			callback.m_info = m_lastConnectionInfo;
			m_gameNetworkingMessagesOwner.m_gameNetworkingSockets.QueueCallback( callback, g_Config_Callback_MessagesSessionFailed.Get() );
		}

		// Clean up the connection.
		CGameNetworkConnectionBase *pConn = m_pConnection;
		UnlinkConnection();
		pConn->ConnectionState_FinWait();
	}

	m_bConnectionStateChanged = false;
}

void GameNetworkingMessagesSession::Think( GameNetworkingMicroseconds usecNow )
{

	// Check on the connection
	CheckConnection( usecNow );

	// Time to idle out the session?
	if ( usecNow >= m_usecIdleTimeout )
	{
		// If we don't have a connection, then we can just self destruct now
		if ( !m_pConnection )
		{
			SpewMsg( "Messages session %s: idle timed out.  Destroying\n", GameNetworkingIdentityRender( m_identityRemote ).c_str() );
			m_gameNetworkingMessagesOwner.DestroySession( m_identityRemote );
			return;
		}

		// Make sure lower level connection is also idle and nothing is buffered.
		if ( m_pConnection->SNP_BHasAnyBufferedRecvData() )
		{
			// The peer hasn't started sending us data.  (Just not a complete message yet.)
			// This is a relatively small race condition.  Keep extending the timeout
			// until either the connection drops, or the full message gets delivered
			SpewMsg( "Messages session %s: connection [%s] is idle timing out, but we have a partial message from our peer.  Assuming a message was sent just at the timeout deadline.   Extending timeout.\n", GameNetworkingIdentityRender( m_identityRemote ).c_str(), m_pConnection->GetDescription() );
			m_usecIdleTimeout = usecNow + k_nMillion;
		}
		else if ( m_pConnection->SNP_BHasAnyUnackedSentReliableData() )
		{
			// We *really* ought to think that the peer has acked all of our data.
			// Because our timeouts are really generous compared to ping times,
			// throughput, and max message size
			AssertMsg2( false, "Messages session %s: connection [%s] is idle timing out.  But we still have unacked sent data?!?  This seems bad\n", GameNetworkingIdentityRender( m_identityRemote ).c_str(), m_pConnection->GetDescription() );
			m_usecIdleTimeout = usecNow + k_nMillion;
		}
		else
		{
			// We're idle.  Nuke the connection.  If the peer has tried to send us any messages,
			// they'll get the notification that we closed the message, and they can resend.
			// the thing is that they should know for sure that no partial messages were delivered,
			// so everything they have queued, they just need to resend.
			SpewMsg( "Messages session %s: idle timing out.  Closing connection [%s] and destroying session\n", GameNetworkingIdentityRender( m_identityRemote ).c_str(), m_pConnection->GetDescription() );
			CloseConnection( k_EGameNetConnectionEnd_P2P_SessionIdleTimeout, "Session Idle Timeout" );

			// Self-destruct
			m_gameNetworkingMessagesOwner.DestroySession( m_identityRemote );
			return;
		}
	}

	ScheduleThink();
}

static void FreeMessageDataWithP2PMessageHeader( GameNetworkingMessage_t *pMsg )
{
	void *hdr = static_cast<P2PMessageHeader *>( pMsg->m_pData ) - 1;
	::free( hdr );
}

void GameNetworkingMessagesSession::ReceivedMessage( CGameNetworkingMessage *pMsg )
{
	// Caller locks this
	g_lockAllRecvMessageQueues.AssertHeldByCurrentThread();

	// Make sure the message is big enough to contain a header
	if ( pMsg->m_cbSize < sizeof(P2PMessageHeader) )
	{
		AssertMsg2( false, "Internal P2P message from %s is %d bytes; that's not big enough for the header!", GameNetworkingIdentityRender( m_identityRemote ).c_str(), pMsg->m_cbSize );
		pMsg->Release();
		return;
	}
	Assert( pMsg->m_pfnFreeData == CGameNetworkingMessage::DefaultFreeData );

	// Process the header
	P2PMessageHeader *hdr = static_cast<P2PMessageHeader *>( pMsg->m_pData );
	pMsg->m_nChannel = LittleDWord( hdr->m_nToChannel );
	pMsg->m_cbSize -= sizeof(P2PMessageHeader);
	pMsg->m_pData = hdr+1;
	pMsg->m_pfnFreeData = FreeMessageDataWithP2PMessageHeader;

	// Add to the session
	pMsg->LinkToQueueTail( &CGameNetworkingMessage::m_links, &m_queueRecvMessages );

	// Mark channel as open
	m_mapOpenChannels.Insert( pMsg->m_nChannel, true );

	// Add to end of channel queue
	CGameNetworkingMessages::Channel *pChannel = m_gameNetworkingMessagesOwner.FindOrCreateChannel( pMsg->m_nChannel );
	pMsg->LinkToQueueTail( &CGameNetworkingMessage::m_linksSecondaryQueue, &pChannel->m_queueRecvMessages );
}

void GameNetworkingMessagesSession::ConnectionStateChanged( GameNetConnectionStatusChangedCallback_t *pInfo )
{
	GameNetworkingGlobalLock::AssertHeldByCurrentThread();

	// If we are already disassociated from our session, then we don't care.
	if ( !m_pConnection )
	{
		AssertMsg( false, "GameNetworkingMessagesSession::ConnectionStateChanged after detaching from connection?" );
		return;
	}
	Assert( m_pConnection->m_hConnectionSelf == pInfo->m_hConn );

	ConnectionScopeLock connectionLock( *m_pConnection );

	// If we're dead (about to be destroyed, entering finwait, etc, then unlink from session)
	EGameNetworkingConnectionState eNewAPIState = pInfo->m_info.m_eState;
	if ( eNewAPIState == k_EGameNetworkingConnectionState_None )
	{
		UnlinkConnection();
		return;
	}

	// Reset idle timeout if we connect
	if ( eNewAPIState == k_EGameNetworkingConnectionState_Connecting || eNewAPIState == k_EGameNetworkingConnectionState_Connected || eNewAPIState == k_EGameNetworkingConnectionState_FindingRoute )
	{
		MarkUsed( GameNetworkingSockets_GetLocalTimestamp() );
		if ( eNewAPIState == k_EGameNetworkingConnectionState_Connected )
			m_bConnectionWasEverConnected = true;
	}

	// Schedule an immediate wakeup of the session, so we can deal with this
	// at a safe time
	m_bConnectionStateChanged = true;
	SetNextThinkTimeASAP();
}

void GameNetworkingMessagesSession::LinkConnection( CGameNetworkConnectionBase *pConn )
{
	UnlinkConnection();
	if ( !pConn )
		return;
	Assert( !g_mapSessionsByConnection.HasElement( pConn->m_hConnectionSelf ) );
	m_pConnection = pConn;
	g_mapSessionsByConnection.InsertOrReplace( pConn->m_hConnectionSelf, this );

	m_bConnectionStateChanged = true;
	m_bConnectionWasEverConnected = false;
	SetNextThinkTimeASAP();
	MarkUsed( GameNetworkingSockets_GetLocalTimestamp() );

	pConn->SetPollGroup( m_gameNetworkingMessagesOwner.m_pPollGroup );
	UpdateConnectionInfo();
}

void GameNetworkingMessagesSession::UnlinkConnection()
{
	if ( !m_pConnection )
		return;

	int h = g_mapSessionsByConnection.Find( m_pConnection->m_hConnectionSelf );
	if ( h == g_mapSessionsByConnection.InvalidIndex() || g_mapSessionsByConnection[h] != this )
	{
		AssertMsg( false, "Messages session bookkeeping bug" );
	}
	else
	{
		g_mapSessionsByConnection[h] = nullptr; // just for grins
		g_mapSessionsByConnection.RemoveAt( h );
	}

	m_pConnection = nullptr;
	m_bConnectionStateChanged = true;
	SetNextThinkTimeASAP();
}

#ifdef DBGFLAG_VALIDATE

void GameNetworkingMessagesSession::Validate( CValidator &validator, const char *pchName )
{
	ValidateRecursive( m_mapOpenChannels );
	// FIXME: m_queueRecvMessages
}

#endif

} // namespace GameNetworkingSocketsLib
using namespace GameNetworkingSocketsLib;

#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES

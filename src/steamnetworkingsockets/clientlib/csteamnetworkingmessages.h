//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef CSTEAMNETWORKINGMESSAGES_H
#define CSTEAMNETWORKINGMESSAGES_H
#pragma once

#include <tier1/utlhashmap.h>
#include <steam/steamnetworkingtypes.h>
#include <steam/isteamnetworkingmessages.h>
#include "steamnetworkingsockets_connections.h"

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES

#if defined( STEAMNETWORKINGSOCKETS_STEAMCLIENT ) || defined( STEAMNETWORKINGSOCKETS_STREAMINGCLIENT )
	#include <steam/iclientnetworkingmessages.h>
#else
	typedef IGameNetworkingMessages IClientNetworkingMessages;
#endif

class CMsgSteamDatagramConnectRequest;

namespace GameNetworkingSocketsLib {

class CGameNetworkingSockets;
class CGameNetworkingMessage;
class CGameNetworkingMessages;

/////////////////////////////////////////////////////////////////////////////
//
// Steam API interfaces
//
/////////////////////////////////////////////////////////////////////////////

struct GameNetworkingMessagesSession : public IThinker
{
	GameNetworkingMessagesSession( const GameNetworkingIdentity &identityRemote, CGameNetworkingMessages &steamNetworkingP2P );
	virtual ~GameNetworkingMessagesSession();

	GameNetworkingIdentity m_identityRemote;
	CGameNetworkingMessages &m_steamNetworkingMessagesOwner;
	CGameNetworkConnectionBase *m_pConnection; // active connection, if any.  Might be NULL!

	/// Queue of inbound messages
	GameNetworkingMessageQueue m_queueRecvMessages;

	CUtlHashMap<int,bool,std::equal_to<int>,std::hash<int>> m_mapOpenChannels;

	/// If we get tot his time, the session has been idle
	/// and we should clean it up.
	GameNetworkingMicroseconds m_usecIdleTimeout;

	/// True if the connection has changed state and we need to check on it
	bool m_bConnectionStateChanged;

	/// True if the current connection ever managed to go fully connected
	bool m_bConnectionWasEverConnected;

	/// Most recent info about the connection.
	GameNetConnectionInfo_t m_lastConnectionInfo;
	GameNetworkingQuickConnectionStatus m_lastQuickStatus;

	/// Close the connection with the specified reason info
	void CloseConnection( int nReason, const char *pszDebug );

	/// Record that we have been used
	void MarkUsed( GameNetworkingMicroseconds usecNow );

	/// Ensure that we are scheduled to wake up and get service
	/// at the next time it looks like we might need to do something
	void ScheduleThink();

	// Implements IThinker
	virtual void Think( GameNetworkingMicroseconds usecNow ) override;

	/// Check on the connection state
	void CheckConnection( GameNetworkingMicroseconds usecNow );

	void UpdateConnectionInfo();

	void LinkConnection( CGameNetworkConnectionBase *pConn );
	void UnlinkConnection();

	void ReceivedMessage( CGameNetworkingMessage *pMsg );
	void ConnectionStateChanged( GameNetConnectionStatusChangedCallback_t *pInfo );

	#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
	#endif
};

class CGameNetworkingMessages : public IClientNetworkingMessages
{
public:
	STEAMNETWORKINGSOCKETS_DECLARE_CLASS_OPERATOR_NEW
	CGameNetworkingMessages( CGameNetworkingSockets &steamNetworkingSockets );
	virtual ~CGameNetworkingMessages();

	bool BInit();
	void FreeResources();

	// Implements IGameNetworkingMessages
	virtual EResult SendMessageToUser( const GameNetworkingIdentity &identityRemote, const void *pubData, uint32 cubData, int nSendFlags, int nChannel ) override;
	virtual int ReceiveMessagesOnChannel( int nChannel, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages ) override;
	virtual bool AcceptSessionWithUser( const GameNetworkingIdentity &identityRemote ) override;
	virtual bool CloseSessionWithUser( const GameNetworkingIdentity &identityRemote ) override;
	virtual bool CloseChannelWithUser( const GameNetworkingIdentity &identityRemote, int nChannel ) override;
	virtual EGameNetworkingConnectionState GetSessionConnectionInfo( const GameNetworkingIdentity &identityRemote, GameNetConnectionInfo_t *pConnectionInfo, GameNetworkingQuickConnectionStatus *pQuickStatus ) override;

	#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName ) override;
	#endif

	void NewConnection( CGameNetworkConnectionBase *pConn );

	CGameNetworkingSockets &m_steamNetworkingSockets;

	struct Channel
	{
		Channel();
		~Channel();

		GameNetworkingMessageQueue m_queueRecvMessages;
	};

	CGameNetworkListenSocketBase *m_pListenSocket = nullptr;
	CGameNetworkPollGroup *m_pPollGroup = nullptr;

	Channel *FindOrCreateChannel( int nChannel );
	void DestroySession( const GameNetworkingIdentity &identityRemote );

	void PollMessages( GameNetworkingMicroseconds usecNow );

private:

	GameNetworkingMessagesSession *FindSession( const GameNetworkingIdentity &identityRemote, ConnectionScopeLock &scopeLock );
	GameNetworkingMessagesSession *FindOrCreateSession( const GameNetworkingIdentity &identityRemote, ConnectionScopeLock &scopeLock );

	CUtlHashMap< GameNetworkingIdentity, GameNetworkingMessagesSession *, std::equal_to<GameNetworkingIdentity>, GameNetworkingIdentityHash > m_mapSessions;
	CUtlHashMap<int,Channel*,std::equal_to<int>,std::hash<int>> m_mapChannels;

	static void ConnectionStatusChangedCallback( GameNetConnectionStatusChangedCallback_t *pInfo );
};

} // namespace GameNetworkingSocketsLib

#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_STEAMNETWORKINGMESSAGES

#endif // CSTEAMNETWORKINGMESSAGES_H

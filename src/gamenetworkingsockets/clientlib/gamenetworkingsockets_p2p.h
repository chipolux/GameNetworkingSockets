//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef STEAMNETWORKINGSOCKETS_P2P_H
#define STEAMNETWORKINGSOCKETS_P2P_H
#pragma once

#include <gns/gamenetworkingcustomsignaling.h>
#include "gamenetworkingsockets_connections.h"
#include "cgamenetworkingsockets.h"

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
	#include <gamedatagram_messages_sdr.pb.h>
#endif

class CMsgSteamDatagramConnectRequest;

namespace GameNetworkingSocketsLib {

/// Special disconnection reason code that is used in signals
/// to indicate "no connection"
const uint32 k_EGameNetConnectionEnd_Internal_P2PNoConnection = 9999;

// If we are the "controlled" agent, add this penalty to routes
// other than the one that are not the one the controlling agent
// has selected
constexpr int k_nRoutePenaltyNotNominated = 100;
constexpr int k_nRoutePenaltyNeedToConfirmConnectivity = 10000;
constexpr int k_nRoutePenaltyNotLan = 10; // Any route that appears to be a LAN route gets a bonus.  (Actually, all others are penalized)
constexpr int k_nRoutePenaltyNotSelectedOverride = 4000;

// Values for P2PTRansportOverride config value
constexpr int k_nP2P_TransportOverride_None = 0;
constexpr int k_nP2P_TransportOverride_SDR = 1;
constexpr int k_nP2P_TransportOverride_ICE = 2;

constexpr int k_nICECloseCode_Local_NotCompiled = k_EGameNetConnectionEnd_Local_Max;
constexpr int k_nICECloseCode_Local_UserNotEnabled = k_EGameNetConnectionEnd_Local_Max-1;
constexpr int k_nICECloseCode_Local_Special = k_EGameNetConnectionEnd_Local_Max-2; // Not enabled because we are forcing a particular transport that isn't ICE
constexpr int k_nICECloseCode_Aborted = k_EGameNetConnectionEnd_Local_Max-2;
constexpr int k_nICECloseCode_Remote_NotEnabled = k_EGameNetConnectionEnd_Remote_Max;

class CConnectionTransportP2PSDR;
class CConnectionTransportToSDRServer;
class CConnectionTransportFromSDRClient;
class CConnectionTransportP2PICE;
class CGameNetworkListenSocketSDRServer;
struct CachedRelayAuthTicket;

//-----------------------------------------------------------------------------
/// Base class for listen sockets where the client will connect to us using
/// our identity and a "virtual port".
///
/// Current derived classes are true "P2P" connections, and connections to
/// servers hosted in known data centers.

class CGameNetworkListenSocketP2P : public CGameNetworkListenSocketBase
{
public:
	CGameNetworkListenSocketP2P( CGameNetworkingSockets *pGameNetworkingSocketsInterface );

	// CGameNetworkListenSocketBase overrides
	virtual bool BSupportsSymmetricMode() override { return true; }

	/// Setup
	virtual bool BInit( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg );

	inline int LocalVirtualPort() const
	{
		Assert( m_connectionConfig.m_LocalVirtualPort.IsLocked() );
		return m_connectionConfig.m_LocalVirtualPort.m_data;
	}

	// Listen sockets for hosted dedicated server connections derive from this class.
	// This enum tells what methods we will allow clients to connect to us.
	#ifdef SDR_ENABLE_HOSTED_SERVER
	enum EHostedDedicatedServer
	{
		k_EHostedDedicatedServer_Not, // We're an ordinary P2P listen socket
		k_EHostedDedicatedServer_Auto, // We're hosted dedicated server, and we allow "P2P" connections thorugh signaling.  We'll issue tickets to clients signed with our cert.
		k_EHostedDedicatedServer_TicketsOnly, // We're hosted dedicated server, and clients must have a ticket.  We won't issue them.
	};
	EHostedDedicatedServer m_eHostedDedicatedServer = k_EHostedDedicatedServer_Not;
	#endif

protected:
	virtual ~CGameNetworkListenSocketP2P();
};

/// Mixin base class for different P2P transports.
class CConnectionTransportP2PBase
{
public:
	// Virtual base classes.  (We don't directly derive, since we are a mixin,
	// but all classes that derive from us will derive from these base classes.)
	CConnectionTransport *const m_pSelfAsConnectionTransport;

	const char *const m_pszP2PTransportDebugName;

	/// True if we need to take aggressive action to confirm
	/// end-to-end connectivity.  This will be the case when
	/// doing initial route finding, or if we aren't sure about
	/// end-to-end connectivity because we lost all of our
	/// sessions, etc.  Once we get some data packets, we set
	/// this flag to false.
	bool m_bNeedToConfirmEndToEndConnectivity;

	// Some basic stats tracking about ping times.  Currently these only track the pings
	// explicitly sent at this layer.  Ideally we would hook into the SNP code, because
	// almost every data packet we send contains ping-related information.
	PingTrackerForRouteSelection m_pingEndToEnd;
	GameNetworkingMicroseconds m_usecEndToEndInFlightReplyTimeout;
	int m_nReplyTimeoutsSinceLastRecv;
	int m_nKeepTryingToPingCounter;
	GameNetworkingMicroseconds m_usecWhenSelected; // nonzero if we are the current transport
	GameNetworkingMicroseconds m_usecTimeSelectedAccumulator; // How much time have we spent selected, not counting the current activation

	GameNetworkingMicroseconds CalcTotalTimeSelected( GameNetworkingMicroseconds usecNow ) const;

	struct P2PRouteQualityMetrics
	{
		// Scores based only on ping times.
		int m_nScoreCurrent;
		int m_nScoreMin;
		int m_nScoreMax;

		// Sum of all penalties
		int m_nTotalPenalty;

		// Number of recent valid ping collection intervals.
		// (See PingTrackerForRouteSelection)
		int m_nBucketsValid;

		inline void SetInvalid()
		{
			m_nScoreCurrent = k_nRouteScoreHuge;
			m_nScoreMin = k_nRouteScoreHuge;
			m_nScoreMax = k_nRouteScoreHuge;
			m_nTotalPenalty = 0;
			m_nBucketsValid = 0;
		}

	};
	P2PRouteQualityMetrics m_routeMetrics;

	void P2PTransportTrackRecvEndToEndPacket( GameNetworkingMicroseconds usecNow )
	{
		m_usecEndToEndInFlightReplyTimeout = 0;
		m_nReplyTimeoutsSinceLastRecv = 0;
	}
	void P2PTransportTrackSentEndToEndPingRequest( GameNetworkingMicroseconds usecNow, bool bAllowDelayedReply );
	void P2PTransportEndToEndConnectivityConfirmed( GameNetworkingMicroseconds usecNow );
	void P2PTransportEndToEndConnectivityNotConfirmed( GameNetworkingMicroseconds usecNow );

	// Populate m_routeMetrics.  If we're not really available, then the metrics should be set to a huge score
	virtual void P2PTransportUpdateRouteMetrics( GameNetworkingMicroseconds usecNow ) = 0;

	inline void EnsureP2PTransportThink( GameNetworkingMicroseconds usecWhen )
	{
		m_scheduleP2PTransportThink.EnsureMinScheduleTime( this, &CConnectionTransportP2PBase::P2PTransportThink, usecWhen );
	}

	// Try to take the connection lock.  Needed so we can use ScheduledMethodThinkerLockable
	bool TryLock() { return m_pSelfAsConnectionTransport->m_connection.TryLock(); }
	void Unlock() const { return m_pSelfAsConnectionTransport->m_connection.Unlock(); }

protected:
	CConnectionTransportP2PBase( const char *pszDebugName, CConnectionTransport *pSelfBase );
	virtual ~CConnectionTransportP2PBase();

	// Shortcut to get connection and upcast
	CGameNetworkConnectionP2P &Connection() const;

	virtual void P2PTransportThink( GameNetworkingMicroseconds usecNow );
	ScheduledMethodThinkerLockable<CConnectionTransportP2PBase> m_scheduleP2PTransportThink;
};

/// A peer-to-peer connection that can use different types of underlying transport
class CGameNetworkConnectionP2P : public CGameNetworkConnectionBase
{
public:
	CGameNetworkConnectionP2P( CGameNetworkingSockets *pGameNetworkingSocketsInterface, ConnectionScopeLock &scopeLock );

	/// Start connecting to a remote peer at the specified virtual port
	bool BInitConnect(
		IGameNetworkingConnectionSignaling *pSignaling,
		const GameNetworkingIdentity *pIdentityRemote, int nRemoteVirtualPort,
		int nOptions, const GameNetworkingConfigValue_t *pOptions,
		CGameNetworkConnectionP2P **pOutMatchingSymmetricConnection,
		SteamDatagramErrMsg &errMsg
	);

	/// Begin accepting a P2P connection
	virtual bool BBeginAcceptFromSignal(
		const CMsgGameNetworkingP2PRendezvous_ConnectRequest &msgConnectRequest,
		SteamDatagramErrMsg &errMsg,
		GameNetworkingMicroseconds usecNow
	);

	/// Called on a connection that we initiated, when we have a matching symmetric incoming connection,
	/// and we need to change the role of our connection to be "server"
	void ChangeRoleToServerAndAccept( const CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow );

	// CGameNetworkConnectionBase overrides
	virtual void FreeResources() override;
	virtual EResult AcceptConnection( GameNetworkingMicroseconds usecNow ) override;
	virtual void GetConnectionTypeDescription( ConnectionTypeDescription_t &szDescription ) const override;
	virtual void ThinkConnection( GameNetworkingMicroseconds usecNow ) override;
	virtual GameNetworkingMicroseconds ThinkConnection_ClientConnecting( GameNetworkingMicroseconds usecNow ) override;
	virtual void DestroyTransport() override;
	virtual CGameNetworkConnectionP2P *AsGameNetworkConnectionP2P() override;
	virtual void ConnectionStateChanged( EGameNetworkingConnectionState eOldState ) override;
	virtual void ProcessSNPPing( int msPing, RecvPacketContext_t &ctx ) override;
	virtual bool BSupportsSymmetricMode() override;
	EGameNetConnectionEnd CheckRemoteCert( const CertAuthScope *pCACertAuthScope, GameNetworkingErrMsg &errMsg ) override;

	void SendConnectOKSignal( GameNetworkingMicroseconds usecNow );
	void SendConnectionClosedSignal( GameNetworkingMicroseconds usecNow );
	void SendNoConnectionSignal( GameNetworkingMicroseconds usecNow );

	void ScheduleSendSignal( const char *pszReason );
	void QueueSignalReliableMessage( CMsgGameNetworkingP2PRendezvous_ReliableMessage &&msg, const char *pszDebug );

	/// Given a partially-completed CMsgGameNetworkingP2PRendezvous, finish filling out
	/// the required fields, and send it to the peer via the signaling mechanism
	void SetRendezvousCommonFieldsAndSendSignal( CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow, const char *pszDebugReason );

	bool ProcessSignal( const CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow );
	void ProcessSignal_ConnectOK( const CMsgGameNetworkingP2PRendezvous_ConnectOK &msgConnectOK, GameNetworkingMicroseconds usecNow );

	// Return true if we are the "controlling" peer, in the ICE sense of the term.
	// That is, the agent who will primarily make the route decisions, with the
	// controlled agent accepting whatever routing decisions are made, when possible.
	inline bool IsControllingAgent() const
	{

		// Special SDR client connection?
		if ( IsSDRHostedServerClient() )
			return true;

		// Ordinary true peer-to-peer connection.
		//
		// For now, the "server" will always be the controlling agent.
		// This is the opposite of the ICE convention, but we had some
		// reasons for the initial use case to do it this way.  We can
		// plumb through role negotiation if we need to change this.
		return m_bConnectionInitiatedRemotely;
	}

	/// Virtual port on the remote host.  If connection was initiated locally, this will always be valid.
	/// If initiated remotely, we don't need to know except for the purpose of purposes of symmetric connection
	/// matching.  If the peer didn't specify when attempting to connect, we will assume that it is the same
	/// as the local virtual port.
	int m_nRemoteVirtualPort;

	/// local virtual port is a configuration option
	inline int LocalVirtualPort() const { return m_connectionConfig.m_LocalVirtualPort.Get(); }

	/// Handle to our entry in g_mapIncomingP2PConnections, or -1 if we're not in the map
	int m_idxMapP2PConnectionsByRemoteInfo;

	/// How to send signals to the remote host for this
	IGameNetworkingConnectionSignaling *m_pSignaling;

	//
	// Different transports
	//

	// Steam datagram relay
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

		// Peer to peer, over SDR
		CConnectionTransportP2PSDR *m_pTransportP2PSDR;
		CMsgGameNetworkingP2PSDRRoutingSummary m_msgSDRRoutingSummary;
	#endif

	// Client connecting to hosted dedicated server over SDR.  These are not really
	// "Peer to peer" connections.  In a previous iteration of the code these were
	// a totally separate connection class, because we always knew when initiating
	// the connection that it was going to be this type.  However, now these connections
	// may begin their life as an ordinary P2P connection, and only discover from a signal
	// from the peer that it is a server in a hosted data center.  Then they will switch to
	// use the special-case optimized transport.
	#ifdef SDR_ENABLE_HOSTED_CLIENT
		CConnectionTransportToSDRServer *m_pTransportToSDRServer;
		bool BInitConnectToSDRServer( const GameNetworkingIdentity &identityTarget, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions, GameNetworkingErrMsg &errMsg );
		bool BSelectTransportToSDRServerFromSignal( const CMsgGameNetworkingP2PRendezvous &msg );
		void InternalCreateTransportToSDRServer( const CachedRelayAuthTicket &authTicket );
		inline bool IsSDRHostedServerClient() const
		{
			if ( m_pTransportToSDRServer )
			{
				Assert( m_vecAvailableTransports.empty() );
				return true;
			}
			return false;
		}
	#else
		inline bool IsSDRHostedServerClient() const { return false; }
	#endif

	// We are the server in special hosted data center
	#ifdef SDR_ENABLE_HOSTED_SERVER
		CConnectionTransportFromSDRClient *m_pTransportFromSDRClient;
		inline bool IsSDRHostedServer() const
		{
			if ( m_pTransportFromSDRClient )
			{
				Assert( m_vecAvailableTransports.empty() );
				return true;
			}
			return false;
		}
	#else
		inline bool IsSDRHostedServer() const { return false; }
	#endif

	// ICE (direct NAT punch)
	#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE

		// ICE transport that we are using, if any
		CConnectionTransportP2PICE *m_pTransportICE;

		// If ICE transport needs to self-destruct, we move it here, and clear
		// m_pTransportICE.  Then it will be deleted at a safe time.
		CConnectionTransportP2PICE *m_pTransportICEPendingDelete;

		// When we receive a connection from peer, we need to wait for the app
		// to accept it.  During that time we may need to pend any ICE messages
		std_vector<CMsgICERendezvous> m_vecPendingICEMessages;

		// Summary of connection.  Note in particular that the failure reason (if any)
		// is here.
		CMsgGameNetworkingICESessionSummary m_msgICESessionSummary;

		// Detailed failure reason string.
		ConnectionEndDebugMsg m_szICECloseMsg;

		void ICEFailed( int nReasonCode, const char *pszReason );
		inline int GetICEFailureCode() const { return m_msgICESessionSummary.failure_reason_code(); }
		void GuessICEFailureReason( EGameNetConnectionEnd &nReasonCode, ConnectionEndDebugMsg &msg, GameNetworkingMicroseconds usecNow );
	#else
		inline int GetICEFailureCode() const { return k_nICECloseCode_Local_NotCompiled; }
	#endif

	/// Sometimes it's nice to have all existing options in a list.
	/// This list might be empty!  If we are in a special situation where
	/// the current transport is not really a "P2P" transport
	vstd::small_vector< CConnectionTransportP2PBase *, 3 > m_vecAvailableTransports;

	/// Currently selected transport, as a P2P transport.
	/// Always the same as m_pTransport, but as CConnectionTransportP2PBase
	/// Will be NULL if no transport is selected, or if we're in a special
	/// case where the transport isn't really "P2P"
	CConnectionTransportP2PBase *m_pCurrentTransportP2P;

	/// Which transport does it look like our peer is using?
	CConnectionTransportP2PBase *m_pPeerSelectedTransport;
	void SetPeerSelectedTransport( CConnectionTransportP2PBase *pPeerSelectedTransport )
	{
		if ( m_pPeerSelectedTransport != pPeerSelectedTransport )
		{
			m_pPeerSelectedTransport = pPeerSelectedTransport;
			PeerSelectedTransportChanged();
		}
	}

	/// Initialize SDR transport, as appropriate
	virtual bool BInitSDRTransport( GameNetworkingErrMsg &errMsg );

	// Check if user permissions for the remote host are allowed, then
	// create ICE.  Also, if the connection was initiated remotely,
	// we will create an offer
	void CheckInitICE();

	// Check if we pended ICE deletion, then do so now
	void CheckCleanupICE();

	// If we don't already have a failure code for ice, set one now.
	void EnsureICEFailureReasonSet( GameNetworkingMicroseconds usecNow );

	//
	// Transport evaluation and selection
	//

	GameNetworkingMicroseconds m_usecWhenStartedFindingRoute;

	GameNetworkingMicroseconds m_usecNextEvaluateTransport;

	/// True if we should be "sticky" to the current transport.
	/// When major state changes happen, we clear this flag
	/// and evaluate from scratch with no stickiness
	bool m_bTransportSticky;

	void ThinkSelectTransport( GameNetworkingMicroseconds usecNow );
	void TransportEndToEndConnectivityChanged( CConnectionTransportP2PBase *pTransportP2P, GameNetworkingMicroseconds usecNow );
	void SelectTransport( CConnectionTransportP2PBase *pTransport, GameNetworkingMicroseconds usecNow );

	void UpdateTransportSummaries( GameNetworkingMicroseconds usecNow );

	// FIXME - UDP transport for LAN discovery, so P2P works without any signaling

	inline int LogLevel_P2PRendezvous() const { return m_connectionConfig.m_LogLevel_P2PRendezvous.Get(); }

	static CGameNetworkConnectionP2P *FindDuplicateConnection( CGameNetworkingSockets *pInterfaceLocal, int nLocalVirtualPort, const GameNetworkingIdentity &identityRemote, int nRemoteVirtualPort, bool bOnlySymmetricConnections, CGameNetworkConnectionP2P *pIgnore );

	void RemoveP2PConnectionMapByRemoteInfo();
	bool BEnsureInP2PConnectionMapByRemoteInfo( SteamDatagramErrMsg &errMsg );

protected:
	virtual ~CGameNetworkConnectionP2P();

	/// Shared init
	bool BInitP2PConnectionCommon( GameNetworkingMicroseconds usecNow, int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg );

	virtual void PopulateRendezvousMsgWithTransportInfo( CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow );

private:

	struct OutboundMessage
	{
		uint32 m_nID;
		int m_cbSerialized;
		GameNetworkingMicroseconds m_usecRTO; // Retry timeout
		CMsgGameNetworkingP2PRendezvous_ReliableMessage m_msg;
	};
	std_vector< OutboundMessage > m_vecUnackedOutboundMessages; // outbound messages that have not been acked

	const char *m_pszNeedToSendSignalReason;
	GameNetworkingMicroseconds m_usecSendSignalDeadline;
	uint32 m_nLastSendRendesvousMessageID;
	uint32 m_nLastRecvRendesvousMessageID;

	// Really destroy ICE now
	void DestroyICENow();

	void PeerSelectedTransportChanged();
};

inline CGameNetworkConnectionP2P &CConnectionTransportP2PBase::Connection() const
{
	return *assert_cast<CGameNetworkConnectionP2P *>( &m_pSelfAsConnectionTransport->m_connection );
}

} // namespace GameNetworkingSocketsLib

#endif // STEAMNETWORKINGSOCKETS_P2P_H

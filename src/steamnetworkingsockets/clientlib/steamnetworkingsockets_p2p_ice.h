//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef STEAMNETWORKINGSOCKETS_P2P_ICE_H
#define STEAMNETWORKINGSOCKETS_P2P_ICE_H
#pragma once

#include "steamnetworkingsockets_p2p.h"
#include "steamnetworkingsockets_udp.h"
#include <mutex>

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE

#include "../../external/steamwebrtc/ice_session.h"

extern "C" CreateICESession_t g_GameNetworkingSockets_CreateICESessionFunc;

namespace GameNetworkingSocketsLib {

constexpr int k_nMinPingTimeLocalTolerance = 5;

class CGameNetworkConnectionP2P;
struct UDPSendPacketContext_t;

/// Transport for peer-to-peer connection using WebRTC
class CConnectionTransportP2PICE final
: public CConnectionTransportUDPBase
, public CConnectionTransportP2PBase
, private IICESessionDelegate
{
public:
	CConnectionTransportP2PICE( CGameNetworkConnectionP2P &connection );
	virtual ~CConnectionTransportP2PICE();

	inline CGameNetworkConnectionP2P &Connection() const { return *assert_cast< CGameNetworkConnectionP2P *>( &m_connection ); }
	inline IGameNetworkingConnectionSignaling *Signaling() const { return Connection().m_pSignaling; }

	void Init();

	// CConnectionTransport overrides
	virtual void TransportPopulateConnectionInfo( GameNetConnectionInfo_t &info ) const override;
	virtual void GetDetailedConnectionStatus( GameNetworkingDetailedConnectionStatus &stats, GameNetworkingMicroseconds usecNow ) override;
	virtual void TransportFreeResources() override;
	virtual bool BCanSendEndToEndData() const override;

	// CConnectionTransportP2PBase
	virtual void P2PTransportUpdateRouteMetrics( GameNetworkingMicroseconds usecNow ) override;
	virtual void P2PTransportThink( GameNetworkingMicroseconds usecNow ) override;

	/// Fill in SDR-specific fields to signal
	void PopulateRendezvousMsg( CMsgGameNetworkingP2PRendezvous &msg, GameNetworkingMicroseconds usecNow );
	void RecvRendezvous( const CMsgICERendezvous &msg, GameNetworkingMicroseconds usecNow );

	inline int LogLevel_P2PRendezvous() const { return m_connection.m_connectionConfig.m_LogLevel_P2PRendezvous.Get(); }

	// In certain circumstances we may need to buffer packets
	ShortDurationLock m_mutexPacketQueue;
	CUtlBuffer m_bufPacketQueue;

	//EICECandidateType m_eCurrentRouteLocalCandidateType;
	//EICECandidateType m_eCurrentRouteRemoteCandidateType;
	GameNetworkingIPAddr m_currentRouteRemoteAddress;
	EGameNetTransportKind m_eCurrentRouteKind;
	int m_nAllowedCandidateTypes; // k_EICECandidate_xxx

private:
	IICESession *m_pICESession;

	// Implements IICESessionDelegate
	virtual void Log( IICESessionDelegate::ELogPriority ePriority, const char *pszMessageFormat, ... ) override;
	virtual void OnData( const void *pData, size_t nSize ) override;
	virtual void OnLocalCandidateGathered( EICECandidateType eType, const char *pszCandidate ) override;
	virtual void OnWritableStateChanged() override;
	virtual void OnRouteChanged() override;

	void RouteOrWritableStateChanged();
	void UpdateRoute();

	void DrainPacketQueue( GameNetworkingMicroseconds usecNow );
	void ProcessPacket( const uint8_t *pData, int cbPkt, GameNetworkingMicroseconds usecNow );

	// Implements CConnectionTransportUDPBase
	virtual bool SendPacket( const void *pkt, int cbPkt ) override;
	virtual bool SendPacketGather( int nChunks, const iovec *pChunks, int cbSendTotal ) override;
	virtual void TrackSentStats( UDPSendPacketContext_t &ctx ) override;
	virtual void RecvValidUDPDataPacket( UDPRecvPacketContext_t &ctx ) override;
};

} // namespace GameNetworkingSocketsLib

#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_ICE

#endif // STEAMNETWORKINGSOCKETS_P2P_ICE_H

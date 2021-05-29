//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef CSTEAMNETWORKINGSOCKETS_H
#define CSTEAMNETWORKINGSOCKETS_H
#pragma once

#include <time.h>
#include <mutex>
#include <gns/igamenetworkingsockets.h>
#include <gns/igamenetworkingutils.h>

#if defined( STEAMNETWORKINGSOCKETS_STEAMCLIENT ) || defined( STEAMNETWORKINGSOCKETS_STREAMINGCLIENT )
	#include "../../common/gns/iclientnetworkingsockets.h"
	#include "../../common/gns/iclientnetworkingutils.h"
	#define ICLIENTNETWORKING_OVERRIDE override
#else
	typedef IGameNetworkingSockets IClientNetworkingSockets;
	typedef IGameNetworkingUtils IClientNetworkingUtils;
	#define ICLIENTNETWORKING_OVERRIDE
#endif

#include "gamenetworkingsockets_connections.h"

namespace GameNetworkingSocketsLib {

class CGameNetworkingUtils;
class CGameNetworkListenSocketP2P;

/////////////////////////////////////////////////////////////////////////////
//
// Steam API interfaces
//
/////////////////////////////////////////////////////////////////////////////

class CGameNetworkingSockets : public IClientNetworkingSockets
{
public:
	STEAMNETWORKINGSOCKETS_DECLARE_CLASS_OPERATOR_NEW
	CGameNetworkingSockets( CGameNetworkingUtils *pGameNetworkingUtils );

	CGameNetworkingUtils *const m_pGameNetworkingUtils;
	CMsgSteamDatagramCertificateSigned m_msgSignedCert;
	CMsgSteamDatagramCertificate m_msgCert;
	CECSigningPrivateKey m_keyPrivateKey;
	bool BCertHasIdentity() const;
	virtual bool SetCertificateAndPrivateKey( const void *pCert, int cbCert, void *pPrivateKey, int cbPrivateKey );

	bool BHasAnyConnections() const;
	bool BHasAnyListenSockets() const;
	bool BInitted() const { return m_bHaveLowLevelRef; }

#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
	bool BInitGameNetworkingSockets( const GameNetworkingIdentity *pIdentity, SteamDatagramErrMsg &errMsg );
	void CacheIdentity() { m_identity.SetLocalHost(); }
#else
	virtual void CacheIdentity() = 0;
#endif

	/// Perform cleanup and self-destruct.  Use this instead of
	/// calling operator delete.  This solves some complications
	/// due to calling virtual functions from within destructor.
	void Destroy();
	virtual void FreeResources();

	const GameNetworkingIdentity &InternalGetIdentity()
	{
		if ( m_identity.IsInvalid() )
			CacheIdentity();
		return m_identity;
	}

	template <typename T>
	void QueueCallback( const T& x, void *fnRegisteredFunctionPtr )
	{
		InternalQueueCallback( T::k_iCallback, sizeof(T), &x, fnRegisteredFunctionPtr );
	}

	// Implements IGameNetworkingSockets
	virtual HSteamListenSocket CreateListenSocketIP( const GameNetworkingIPAddr &localAddress, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override;
	virtual HGameNetConnection ConnectByIPAddress( const GameNetworkingIPAddr &adress, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override;
	virtual HSteamListenSocket CreateListenSocketP2P( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override;
	virtual HGameNetConnection ConnectP2P( const GameNetworkingIdentity &identityRemote, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override;
	virtual EResult AcceptConnection( HGameNetConnection hConn ) override;
	virtual bool CloseConnection( HGameNetConnection hConn, int nReason, const char *pszDebug, bool bEnableLinger ) override;
	virtual bool CloseListenSocket( HSteamListenSocket hSocket ) override;
	virtual bool SetConnectionUserData( HGameNetConnection hPeer, int64 nUserData ) override;
	virtual int64 GetConnectionUserData( HGameNetConnection hPeer ) override;
	virtual void SetConnectionName( HGameNetConnection hPeer, const char *pszName ) override;
	virtual bool GetConnectionName( HGameNetConnection hPeer, char *pszName, int nMaxLen ) override;
	virtual EResult SendMessageToConnection( HGameNetConnection hConn, const void *pData, uint32 cbData, int nSendFlags, int64 *pOutMessageNumber ) override;
	virtual void SendMessages( int nMessages, GameNetworkingMessage_t *const *pMessages, int64 *pOutMessageNumberOrResult ) override;
	virtual EResult FlushMessagesOnConnection( HGameNetConnection hConn ) override;
	virtual int ReceiveMessagesOnConnection( HGameNetConnection hConn, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages ) override;
	virtual bool GetConnectionInfo( HGameNetConnection hConn, GameNetConnectionInfo_t *pInfo ) override;
	virtual bool GetQuickConnectionStatus( HGameNetConnection hConn, GameNetworkingQuickConnectionStatus *pStats ) override;
	virtual int GetDetailedConnectionStatus( HGameNetConnection hConn, char *pszBuf, int cbBuf ) override;
	virtual bool GetListenSocketAddress( HSteamListenSocket hSocket, GameNetworkingIPAddr *pAddress ) override;
	virtual bool CreateSocketPair( HGameNetConnection *pOutConnection1, HGameNetConnection *pOutConnection2, bool bUseNetworkLoopback, const GameNetworkingIdentity *pIdentity1, const GameNetworkingIdentity *pIdentity2 ) override;
	virtual bool GetIdentity( GameNetworkingIdentity *pIdentity ) override;

	virtual HGameNetPollGroup CreatePollGroup() override;
	virtual bool DestroyPollGroup( HGameNetPollGroup hPollGroup ) override;
	virtual bool SetConnectionPollGroup( HGameNetConnection hConn, HGameNetPollGroup hPollGroup ) override;
	virtual int ReceiveMessagesOnPollGroup( HGameNetPollGroup hPollGroup, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages ) override; 
	virtual HGameNetConnection ConnectP2PCustomSignaling( IGameNetworkingConnectionSignaling *pSignaling, const GameNetworkingIdentity *pPeerIdentity, int nVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override;
	virtual bool ReceivedP2PCustomSignal( const void *pMsg, int cbMsg, IGameNetworkingSignalingRecvContext *pContext ) override;
	virtual int GetP2P_Transport_ICE_Enable( const GameNetworkingIdentity &identityRemote, int *pOutUserFlags );

	virtual bool GetCertificateRequest( int *pcbBlob, void *pBlob, GameNetworkingErrMsg &errMsg ) override;
	virtual bool SetCertificate( const void *pCertificate, int cbCertificate, GameNetworkingErrMsg &errMsg ) override;
	virtual void ResetIdentity( const GameNetworkingIdentity *pIdentity ) override;

#ifdef STEAMNETWORKINGSOCKETS_STEAMCLIENT
	virtual int ReceiveMessagesOnListenSocketLegacyPollGroup( HSteamListenSocket hSocket, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages ) override;
#endif

	virtual void RunCallbacks() override;

	/// Configuration options that will apply to all connections on this interface
	ConnectionConfig m_connectionConfig;

	/// List of existing CGameNetworkingSockets instances.  This is used, for example,
	/// if we want to initiate a P2P connection to a local identity, we can instead
	/// use a loopback connection.
	static std::vector<CGameNetworkingSockets *> s_vecGameNetworkingSocketsInstances;

	// P2P listen sockets
	CUtlHashMap<int,CGameNetworkListenSocketP2P *,std::equal_to<int>,std::hash<int>> m_mapListenSocketsByVirtualPort;
	CGameNetworkListenSocketP2P *InternalCreateListenSocketP2P( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions );

	CGameNetworkPollGroup *InternalCreatePollGroup( PollGroupScopeLock &scopeLock );

	//
	// Authentication
	//

#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
	virtual bool BCertRequestInFlight() = 0;

	ScheduledMethodThinker<CGameNetworkingSockets> m_scheduleCheckRenewCert;

	/// Platform-specific code to actually obtain a cert
	virtual void BeginFetchCertAsync() = 0;
#else
	inline bool BCertRequestInFlight() { return false; }
#endif

	/// Called in any situation where we need to be able to authenticate, or anticipate
	/// needing to be able to do so soon.  If we don't have one right now, we will begin
	/// taking action to obtain one
	virtual void CheckAuthenticationPrerequisites( GameNetworkingMicroseconds usecNow );
	void AuthenticationNeeded() { CheckAuthenticationPrerequisites( GameNetworkingSockets_GetLocalTimestamp() ); }

	virtual EGameNetworkingAvailability InitAuthentication() override final;
	virtual EGameNetworkingAvailability GetAuthenticationStatus( GameNetAuthenticationStatus_t *pAuthStatus ) override final;
	int GetSecondsUntilCertExpiry() const;

	//
	// Default signaling
	//

	CGameNetworkConnectionBase *InternalConnectP2PDefaultSignaling(
		const GameNetworkingIdentity &identityRemote,
		int nRemoteVirtualPort,
		int nOptions, const GameNetworkingConfigValue_t *pOptions,
		ConnectionScopeLock &scopeLock
	);
	CGameNetworkingMessages *GetGameNetworkingMessages();
	CGameNetworkingMessages *m_pGameNetworkingMessages;

// Stubs if SDR not enabled
#ifndef STEAMNETWORKINGSOCKETS_ENABLE_SDR
	virtual int FindRelayAuthTicketForServer( const GameNetworkingIdentity &identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket *pOutParsedTicket ) override { return 0; }
	virtual HGameNetConnection ConnectToHostedDedicatedServer( const GameNetworkingIdentity &identityTarget, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override { return k_HGameNetConnection_Invalid; }
	virtual uint16 GetHostedDedicatedServerPort() override { return 0; }
	virtual GameNetworkingPOPID GetHostedDedicatedServerPOPID() override { return 0; }
	virtual EResult GetHostedDedicatedServerAddress( SteamDatagramHostedAddress *pRouting ) override { return k_EResultFail; }
	virtual HSteamListenSocket CreateHostedDedicatedServerListenSocket( int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t *pOptions ) override { return k_HGameNetConnection_Invalid; }
	virtual bool ReceivedRelayAuthTicket( const void *pvTicket, int cbTicket, SteamDatagramRelayAuthTicket *pOutParsedTicket ) override { return false; }
	virtual EResult GetGameCoordinatorServerLogin( SteamDatagramGameCoordinatorServerLogin *pLogin, int *pcbSignedBlob, void *pBlob ) override { return k_EResultFail; }
#endif

protected:

	/// Overall authentication status.  Depends on the status of our cert, and the ability
	/// to obtain the CA certs (from the network config)
	GameNetAuthenticationStatus_t m_AuthenticationStatus;

	/// Set new status, dispatch callbacks if it actually changed
	void SetAuthenticationStatus( const GameNetAuthenticationStatus_t &newStatus );

	/// Current status of our attempt to get a certificate
	bool m_bEverTriedToGetCert;
	bool m_bEverGotCert;
	GameNetAuthenticationStatus_t m_CertStatus;

	/// Set cert status, and then update m_AuthenticationStatus and
	/// dispatch any callbacks as needed
	void SetCertStatus( EGameNetworkingAvailability eAvail, const char *pszFmt, ... );
#ifdef STEAMNETWORKINGSOCKETS_CAN_REQUEST_CERT
	void AsyncCertRequestFinished();
	void CertRequestFailed( EGameNetworkingAvailability eCertAvail, EGameNetConnectionEnd nConnectionEndReason, const char *pszMsg );
#endif

	/// Figure out the current authentication status.  And if it has changed, send out callbacks
	virtual void DeduceAuthenticationStatus();

	void InternalInitIdentity();
	void KillConnections();

	GameNetworkingIdentity m_identity;

	struct QueuedCallback
	{
		int nCallback;
		void *fnCallback;
		char data[ sizeof(GameNetConnectionStatusChangedCallback_t) ]; // whatever the biggest callback struct we have is
	};
	std_vector<QueuedCallback> m_vecPendingCallbacks;
	ShortDurationLock m_mutexPendingCallbacks;
	virtual void InternalQueueCallback( int nCallback, int cbCallback, const void *pvCallback, void *fnRegisteredFunctionPtr );

	bool m_bHaveLowLevelRef;
	bool BInitLowLevel( GameNetworkingErrMsg &errMsg );

	CGameNetworkConnectionBase *InternalConnectP2P(
		IGameNetworkingConnectionSignaling *pSignaling,
		const GameNetworkingIdentity *pPeerIdentity,
		int nRemoteVirtualPort,
		int nOptions, const GameNetworkingConfigValue_t *pOptions,
		ConnectionScopeLock &scopeLock
	);
	bool InternalReceivedP2PSignal( const void *pMsg, int cbMsg, IGameNetworkingSignalingRecvContext *pContext, bool bDefaultPlatformSignaling );

	// Protected - use Destroy()
	virtual ~CGameNetworkingSockets();
};

class CGameNetworkingUtils : public IClientNetworkingUtils
{
public:
	STEAMNETWORKINGSOCKETS_DECLARE_CLASS_OPERATOR_NEW
	virtual ~CGameNetworkingUtils();

	virtual GameNetworkingMessage_t *AllocateMessage( int cbAllocateBuffer ) override;

	virtual GameNetworkingMicroseconds GetLocalTimestamp() override;
	virtual void SetDebugOutputFunction( EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc ) override;

	virtual bool SetConfigValue( EGameNetworkingConfigValue eValue,
		EGameNetworkingConfigScope eScopeType, intptr_t scopeObj,
		EGameNetworkingConfigDataType eDataType, const void *pValue ) override;

	virtual EGameNetworkingGetConfigValueResult GetConfigValue(
		EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType,
		intptr_t scopeObj, EGameNetworkingConfigDataType *pOutDataType,
		void *pResult, size_t *cbResult ) override;

	virtual bool GetConfigValueInfo( EGameNetworkingConfigValue eValue,
		const char **pOutName, EGameNetworkingConfigDataType *pOutDataType,
		EGameNetworkingConfigScope *pOutScope, EGameNetworkingConfigValue *pOutNextValue ) override;

	virtual EGameNetworkingConfigValue GetFirstConfigValue() override;

	virtual void GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr &addr, char *buf, size_t cbBuf, bool bWithPort ) override;
	virtual bool GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr *pAddr, const char *pszStr ) override;
	virtual void GameNetworkingIdentity_ToString( const GameNetworkingIdentity &identity, char *buf, size_t cbBuf ) override;
	virtual bool GameNetworkingIdentity_ParseString( GameNetworkingIdentity *pIdentity, const char *pszStr ) override;

	virtual AppId_t GetAppID() override;

	void SetAppID( AppId_t nAppID ) override
	{
		Assert( m_nAppID == 0 || m_nAppID == nAppID );
		m_nAppID = nAppID;
	}

	// Get current time of day, ideally from a source that
	// doesn't depend on the user setting their local clock properly
	virtual time_t GetTimeSecure();

	/// Get a string that describes what version this code is that is running.
	/// (What branch it is, when it was compiled, etc.)
	virtual const char *GetBuildString();
	virtual const char *GetPlatformString();

	// Reset this utils instance for testing
	virtual void TEST_ResetSelf();

	// Stubs if SDR not enabled
#ifndef STEAMNETWORKINGSOCKETS_ENABLE_SDR
	virtual EGameNetworkingAvailability GetRelayNetworkStatus( SteamRelayNetworkStatus_t *pDetails ) override
	{
		if ( pDetails )
		{
			memset( pDetails, 0, sizeof(*pDetails) );
			pDetails->m_eAvail = k_EGameNetworkingAvailability_CannotTry;
			pDetails->m_eAvailAnyRelay = k_EGameNetworkingAvailability_CannotTry;
			pDetails->m_eAvailNetworkConfig = k_EGameNetworkingAvailability_CannotTry;
		}
		return k_EGameNetworkingAvailability_CannotTry;
	}
	virtual bool CheckPingDataUpToDate( float flMaxAgeSeconds ) override { return false; }
	virtual float GetLocalPingLocation( GameNetworkPingLocation_t &result ) override { return -1.0f; }
	virtual int EstimatePingTimeBetweenTwoLocations( const GameNetworkPingLocation_t &location1, const GameNetworkPingLocation_t &location2 ) override { return k_nGameNetworkingPing_Unknown; }
	virtual int EstimatePingTimeFromLocalHost( const GameNetworkPingLocation_t &remoteLocation ) override { return k_nGameNetworkingPing_Unknown; }
	virtual void ConvertPingLocationToString( const GameNetworkPingLocation_t &location, char *pszBuf, int cchBufSize ) override { if ( pszBuf ) *pszBuf = '\0'; }
	virtual bool ParsePingLocationString( const char *pszString, GameNetworkPingLocation_t &result ) override { return false; }
	virtual int GetPingToDataCenter( GameNetworkingPOPID popID, GameNetworkingPOPID *pViaRelayPoP ) override { return k_nGameNetworkingPing_Unknown; }
	virtual int GetDirectPingToPOP( GameNetworkingPOPID popID ) override { return k_nGameNetworkingPing_Unknown; }
	virtual int GetPOPCount() override { return 0; }
	virtual int GetPOPList( GameNetworkingPOPID *list, int nListSz ) override { return 0; }
#endif

protected:
	AppId_t m_nAppID = 0;
};

} // namespace GameNetworkingSocketsLib

#endif // CSTEAMNETWORKINGSOCKETS_H

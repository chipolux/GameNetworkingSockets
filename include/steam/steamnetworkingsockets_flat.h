//====== Copyright Valve Corporation, All rights reserved. ====================
//
// "Flat" interface to GameNetworkingSockets.
//
// Designed to match the auto-generated flat interface in the Steamworks SDK
// (for better or worse...)  It uses plain C linkage, but it is C++ code, and
// is not intended to compile as C code.
//
//=============================================================================

#ifndef STEAMNETWORKINGSOCKETS_FLAT
#define STEAMNETWORKINGSOCKETS_FLAT
#pragma once

#include "steamnetworkingtypes.h"
#include "isteamnetworkingsockets.h"
#include "isteamnetworkingutils.h"
class ISteamNetworkingConnectionSignaling;

typedef uint64 uint64_steamid; // Used when passing or returning CSteamID

// IGameNetworkingSockets
STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingSockets *SteamAPI_GameNetworkingSockets_v009();
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketIP( IGameNetworkingSockets* self, const SteamNetworkingIPAddr & localAddress, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectByIPAddress( IGameNetworkingSockets* self, const SteamNetworkingIPAddr & address, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketP2P( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2P( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityRemote, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_AcceptConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseConnection( IGameNetworkingSockets* self, HSteamNetConnection hPeer, int nReason, const char * pszDebug, bool bEnableLinger );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseListenSocket( IGameNetworkingSockets* self, HSteamListenSocket hSocket );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionUserData( IGameNetworkingSockets* self, HSteamNetConnection hPeer, int64 nUserData );
STEAMNETWORKINGSOCKETS_INTERFACE int64 SteamAPI_IGameNetworkingSockets_GetConnectionUserData( IGameNetworkingSockets* self, HSteamNetConnection hPeer );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SetConnectionName( IGameNetworkingSockets* self, HSteamNetConnection hPeer, const char * pszName );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionName( IGameNetworkingSockets* self, HSteamNetConnection hPeer, char * pszName, int nMaxLen );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_SendMessageToConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn, const void * pData, uint32 cbData, int nSendFlags, int64 * pOutMessageNumber );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SendMessages( IGameNetworkingSockets* self, int nMessages, SteamNetworkingMessage_t *const * pMessages, int64 * pOutMessageNumberOrResult );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_FlushMessagesOnConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionInfo( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetConnectionInfo_t * pInfo );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetQuickConnectionStatus( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetworkingQuickConnectionStatus * pStats );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_GetDetailedConnectionStatus( IGameNetworkingSockets* self, HSteamNetConnection hConn, char * pszBuf, int cbBuf );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetListenSocketAddress( IGameNetworkingSockets* self, HSteamListenSocket hSocket, SteamNetworkingIPAddr * address );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CreateSocketPair( IGameNetworkingSockets* self, HSteamNetConnection * pOutConnection1, HSteamNetConnection * pOutConnection2, bool bUseNetworkLoopback, const SteamNetworkingIdentity * pIdentity1, const SteamNetworkingIdentity * pIdentity2 );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetIdentity( IGameNetworkingSockets* self, SteamNetworkingIdentity * pIdentity );
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingSockets_InitAuthentication( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingSockets_GetAuthenticationStatus( IGameNetworkingSockets* self, SteamNetAuthenticationStatus_t * pDetails );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetPollGroup SteamAPI_IGameNetworkingSockets_CreatePollGroup( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_DestroyPollGroup( IGameNetworkingSockets* self, HSteamNetPollGroup hPollGroup );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionPollGroup( IGameNetworkingSockets* self, HSteamNetConnection hConn, HSteamNetPollGroup hPollGroup );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnPollGroup( IGameNetworkingSockets* self, HSteamNetPollGroup hPollGroup, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedRelayAuthTicket( IGameNetworkingSockets* self, const void * pvTicket, int cbTicket, SteamDatagramRelayAuthTicket * pOutParsedTicket );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_FindRelayAuthTicketForServer( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket * pOutParsedTicket );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectToHostedDedicatedServer( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityTarget, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE uint16 SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPort( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingPOPID SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPOPID( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerAddress( IGameNetworkingSockets* self, SteamDatagramHostedAddress * pRouting );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateHostedDedicatedServerListenSocket( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetGameCoordinatorServerLogin( IGameNetworkingSockets* self, SteamDatagramGameCoordinatorServerLogin * pLoginInfo, int * pcbSignedBlob, void * pBlob );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2PCustomSignaling( IGameNetworkingSockets* self, ISteamNetworkingConnectionSignaling * pSignaling, const SteamNetworkingIdentity * pPeerIdentity, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal( IGameNetworkingSockets* self, const void * pMsg, int cbMsg, ISteamNetworkingSignalingRecvContext * pContext );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetCertificateRequest( IGameNetworkingSockets* self, int * pcbBlob, void * pBlob, SteamNetworkingErrMsg & errMsg );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetCertificate( IGameNetworkingSockets* self, const void * pCertificate, int cbCertificate, SteamNetworkingErrMsg & errMsg );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_RunCallbacks( IGameNetworkingSockets* self );

// IGameNetworkingUtils
STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingUtils *SteamAPI_GameNetworkingUtils_v003();
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingMessage_t * SteamAPI_IGameNetworkingUtils_AllocateMessage( IGameNetworkingUtils* self, int cbAllocateBuffer );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_InitRelayNetworkAccess( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingUtils_GetRelayNetworkStatus( IGameNetworkingUtils* self, SteamRelayNetworkStatus_t * pDetails );
STEAMNETWORKINGSOCKETS_INTERFACE float SteamAPI_IGameNetworkingUtils_GetLocalPingLocation( IGameNetworkingUtils* self, SteamNetworkPingLocation_t & result );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeBetweenTwoLocations( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & location1, const SteamNetworkPingLocation_t & location2 );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeFromLocalHost( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & remoteLocation );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_ConvertPingLocationToString( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & location, char * pszBuf, int cchBufSize );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_ParsePingLocationString( IGameNetworkingUtils* self, const char * pszString, SteamNetworkPingLocation_t & result );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_CheckPingDataUpToDate( IGameNetworkingUtils* self, float flMaxAgeSeconds );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPingToDataCenter( IGameNetworkingUtils* self, SteamNetworkingPOPID popID, SteamNetworkingPOPID * pViaRelayPoP );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetDirectPingToPOP( IGameNetworkingUtils* self, SteamNetworkingPOPID popID );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPCount( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPList( IGameNetworkingUtils* self, SteamNetworkingPOPID * list, int nListSz );
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingMicroseconds SteamAPI_IGameNetworkingUtils_GetLocalTimestamp( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_SetDebugOutputFunction( IGameNetworkingUtils* self, EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueInt32( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, int32 val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueFloat( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, float val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueString( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, const char * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValuePtr( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, void * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueInt32( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, int32 val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueFloat( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, float val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueString( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, const char * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged( IGameNetworkingUtils* self, FnSteamNetConnectionStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged( IGameNetworkingUtils* self, FnSteamNetAuthenticationStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged( IGameNetworkingUtils* self, FnSteamRelayNetworkStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValue( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType eDataType, const void * pArg );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValueStruct( IGameNetworkingUtils* self, const SteamNetworkingConfigValue_t & opt, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj );
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingGetConfigValueResult SteamAPI_IGameNetworkingUtils_GetConfigValue( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType * pOutDataType, void * pResult, size_t * cbResult );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_GetConfigValueInfo( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, const char ** pOutName, ESteamNetworkingConfigDataType * pOutDataType, ESteamNetworkingConfigScope * pOutScope, ESteamNetworkingConfigValue * pOutNextValue );
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingConfigValue SteamAPI_IGameNetworkingUtils_GetFirstConfigValue( IGameNetworkingUtils* self );

// SteamNetworkingIPAddr
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_Clear( SteamNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros( SteamNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv6( SteamNetworkingIPAddr* self, const uint8 * ipv6, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv4( SteamNetworkingIPAddr* self, uint32 nIP, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsIPv4( SteamNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE uint32 SteamAPI_SteamNetworkingIPAddr_GetIPv4( SteamNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost( SteamNetworkingIPAddr* self, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsLocalHost( SteamNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsEqualTo( SteamNetworkingIPAddr* self, const SteamNetworkingIPAddr & x );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_ToString( const SteamNetworkingIPAddr* self, char *buf, size_t cbBuf, bool bWithPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_ParseString( SteamNetworkingIPAddr* self, const char *pszStr );

// SteamNetworkingIdentity
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_Clear( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsInvalid( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetSteamID( SteamNetworkingIdentity* self, uint64_steamid steamID );
STEAMNETWORKINGSOCKETS_INTERFACE uint64_steamid SteamAPI_SteamNetworkingIdentity_GetSteamID( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetSteamID64( SteamNetworkingIdentity* self, uint64 steamID );
STEAMNETWORKINGSOCKETS_INTERFACE uint64 SteamAPI_SteamNetworkingIdentity_GetSteamID64( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetIPAddr( SteamNetworkingIdentity* self, const SteamNetworkingIPAddr & addr );
STEAMNETWORKINGSOCKETS_INTERFACE const SteamNetworkingIPAddr * SteamAPI_SteamNetworkingIdentity_GetIPAddr( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetLocalHost( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsLocalHost( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_SetGenericString( SteamNetworkingIdentity* self, const char * pszString );
STEAMNETWORKINGSOCKETS_INTERFACE const char * SteamAPI_SteamNetworkingIdentity_GetGenericString( SteamNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_SetGenericBytes( SteamNetworkingIdentity* self, const void * data, uint32 cbLen );
STEAMNETWORKINGSOCKETS_INTERFACE const uint8 * SteamAPI_SteamNetworkingIdentity_GetGenericBytes( SteamNetworkingIdentity* self, int & cbLen );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsEqualTo( SteamNetworkingIdentity* self, const SteamNetworkingIdentity & x );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_ToString( const SteamNetworkingIdentity* self, char *buf, size_t cbBuf );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_ParseString( SteamNetworkingIdentity* self, size_t sizeofIdentity, const char *pszStr );

// SteamNetworkingMessage_t
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingMessage_t_Release( SteamNetworkingMessage_t* self );

// SteamDatagramHostedAddress
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_Clear( SteamDatagramHostedAddress* self );
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingPOPID SteamAPI_SteamDatagramHostedAddress_GetPopID( SteamDatagramHostedAddress* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_SetDevAddress( SteamDatagramHostedAddress* self, uint32 nIP, uint16 nPort, SteamNetworkingPOPID popid );
#endif

//
// Special flat functions to make it easier to work with custom signaling
//

typedef bool (*FGameNetworkingSocketsCustomSignaling_SendSignal)( void *ctx, HSteamNetConnection hConn, const SteamNetConnectionInfo_t &info, const void *pMsg, int cbMsg );
typedef void (*FGameNetworkingSocketsCustomSignaling_Release)( void *ctx );

/// Create an ISteamNetworkingConnectionSignaling object from plain C primitives.
STEAMNETWORKINGSOCKETS_INTERFACE ISteamNetworkingConnectionSignaling *SteamAPI_IGameNetworkingSockets_CreateCustomSignaling(
	void *ctx, //< pointer to something useful you understand.  Will be passed to your callbacks.
	FGameNetworkingSocketsCustomSignaling_SendSignal fnSendSignal, //< Callback to send a signal.  See ISteamNetworkingConnectionSignaling::SendSignal
	FGameNetworkingSocketsCustomSignaling_Release fnRelease //< callback to do any cleanup.  See ISteamNetworkingConnectionSignaling::Release.  You can pass NULL if you don't need to do any cleanup.
);

typedef ISteamNetworkingConnectionSignaling * (*FSteamNetworkingCustomSignalingRecvContext_OnConnectRequest)( void *ctx, HSteamNetConnection hConn, const SteamNetworkingIdentity &identityPeer, int nLocalVirtualPort );
typedef void (*FSteamNetworkingCustomSignalingRecvContext_SendRejectionSignal)( void *ctx, const SteamNetworkingIdentity &identityPeer, const void *pMsg, int cbMsg );

/// Same as SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal, but using plain C primitives.
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal2(
	IGameNetworkingSockets* self, const void * pMsg, int cbMsg, //< Same as SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal
	void *ctx, //< pointer to something useful you understand.  Will be passed to your callbacks.
	FSteamNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest, //< callback for sending a signal.  Required.  See ISteamNetworkingSignalingRecvContext::OnConnectRequest
	FSteamNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal //< callback when we wish to actively reject the connection.  Optional, pass NULL if you don't need this.  See ISteamNetworkingSignalingRecvContext::SendRejectionSignal
);

#endif // STEAMNETWORKINGSOCKETS_FLAT

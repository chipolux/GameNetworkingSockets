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
class IGameNetworkingConnectionSignaling;

typedef uint64 uint64_steamid; // Used when passing or returning CSteamID

// IGameNetworkingSockets
STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingSockets *SteamAPI_GameNetworkingSockets_v009();
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketIP( IGameNetworkingSockets* self, const GameNetworkingIPAddr & localAddress, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectByIPAddress( IGameNetworkingSockets* self, const GameNetworkingIPAddr & address, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketP2P( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2P( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityRemote, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_AcceptConnection( IGameNetworkingSockets* self, HGameNetConnection hConn );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseConnection( IGameNetworkingSockets* self, HGameNetConnection hPeer, int nReason, const char * pszDebug, bool bEnableLinger );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseListenSocket( IGameNetworkingSockets* self, HSteamListenSocket hSocket );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionUserData( IGameNetworkingSockets* self, HGameNetConnection hPeer, int64 nUserData );
STEAMNETWORKINGSOCKETS_INTERFACE int64 SteamAPI_IGameNetworkingSockets_GetConnectionUserData( IGameNetworkingSockets* self, HGameNetConnection hPeer );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SetConnectionName( IGameNetworkingSockets* self, HGameNetConnection hPeer, const char * pszName );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionName( IGameNetworkingSockets* self, HGameNetConnection hPeer, char * pszName, int nMaxLen );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_SendMessageToConnection( IGameNetworkingSockets* self, HGameNetConnection hConn, const void * pData, uint32 cbData, int nSendFlags, int64 * pOutMessageNumber );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SendMessages( IGameNetworkingSockets* self, int nMessages, GameNetworkingMessage_t *const * pMessages, int64 * pOutMessageNumberOrResult );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_FlushMessagesOnConnection( IGameNetworkingSockets* self, HGameNetConnection hConn );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnConnection( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionInfo( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetConnectionInfo_t * pInfo );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetQuickConnectionStatus( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetworkingQuickConnectionStatus * pStats );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_GetDetailedConnectionStatus( IGameNetworkingSockets* self, HGameNetConnection hConn, char * pszBuf, int cbBuf );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetListenSocketAddress( IGameNetworkingSockets* self, HSteamListenSocket hSocket, GameNetworkingIPAddr * address );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CreateSocketPair( IGameNetworkingSockets* self, HGameNetConnection * pOutConnection1, HGameNetConnection * pOutConnection2, bool bUseNetworkLoopback, const GameNetworkingIdentity * pIdentity1, const GameNetworkingIdentity * pIdentity2 );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetIdentity( IGameNetworkingSockets* self, GameNetworkingIdentity * pIdentity );
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingSockets_InitAuthentication( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingSockets_GetAuthenticationStatus( IGameNetworkingSockets* self, GameNetAuthenticationStatus_t * pDetails );
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetPollGroup SteamAPI_IGameNetworkingSockets_CreatePollGroup( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_DestroyPollGroup( IGameNetworkingSockets* self, HGameNetPollGroup hPollGroup );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionPollGroup( IGameNetworkingSockets* self, HGameNetConnection hConn, HGameNetPollGroup hPollGroup );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnPollGroup( IGameNetworkingSockets* self, HGameNetPollGroup hPollGroup, GameNetworkingMessage_t ** ppOutMessages, int nMaxMessages );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedRelayAuthTicket( IGameNetworkingSockets* self, const void * pvTicket, int cbTicket, SteamDatagramRelayAuthTicket * pOutParsedTicket );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_FindRelayAuthTicketForServer( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket * pOutParsedTicket );
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectToHostedDedicatedServer( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityTarget, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE uint16 SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPort( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingPOPID SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPOPID( IGameNetworkingSockets* self );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerAddress( IGameNetworkingSockets* self, SteamDatagramHostedAddress * pRouting );
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateHostedDedicatedServerListenSocket( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetGameCoordinatorServerLogin( IGameNetworkingSockets* self, SteamDatagramGameCoordinatorServerLogin * pLoginInfo, int * pcbSignedBlob, void * pBlob );
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2PCustomSignaling( IGameNetworkingSockets* self, IGameNetworkingConnectionSignaling * pSignaling, const GameNetworkingIdentity * pPeerIdentity, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal( IGameNetworkingSockets* self, const void * pMsg, int cbMsg, IGameNetworkingSignalingRecvContext * pContext );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetCertificateRequest( IGameNetworkingSockets* self, int * pcbBlob, void * pBlob, GameNetworkingErrMsg & errMsg );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetCertificate( IGameNetworkingSockets* self, const void * pCertificate, int cbCertificate, GameNetworkingErrMsg & errMsg );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_RunCallbacks( IGameNetworkingSockets* self );

// IGameNetworkingUtils
STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingUtils *SteamAPI_GameNetworkingUtils_v003();
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingMessage_t * SteamAPI_IGameNetworkingUtils_AllocateMessage( IGameNetworkingUtils* self, int cbAllocateBuffer );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_InitRelayNetworkAccess( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingUtils_GetRelayNetworkStatus( IGameNetworkingUtils* self, SteamRelayNetworkStatus_t * pDetails );
STEAMNETWORKINGSOCKETS_INTERFACE float SteamAPI_IGameNetworkingUtils_GetLocalPingLocation( IGameNetworkingUtils* self, GameNetworkPingLocation_t & result );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeBetweenTwoLocations( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & location1, const GameNetworkPingLocation_t & location2 );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeFromLocalHost( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & remoteLocation );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_ConvertPingLocationToString( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & location, char * pszBuf, int cchBufSize );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_ParsePingLocationString( IGameNetworkingUtils* self, const char * pszString, GameNetworkPingLocation_t & result );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_CheckPingDataUpToDate( IGameNetworkingUtils* self, float flMaxAgeSeconds );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPingToDataCenter( IGameNetworkingUtils* self, GameNetworkingPOPID popID, GameNetworkingPOPID * pViaRelayPoP );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetDirectPingToPOP( IGameNetworkingUtils* self, GameNetworkingPOPID popID );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPCount( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPList( IGameNetworkingUtils* self, GameNetworkingPOPID * list, int nListSz );
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingMicroseconds SteamAPI_IGameNetworkingUtils_GetLocalTimestamp( IGameNetworkingUtils* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_SetDebugOutputFunction( IGameNetworkingUtils* self, EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueInt32( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, int32 val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueFloat( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, float val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueString( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, const char * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValuePtr( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, void * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueInt32( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, int32 val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueFloat( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, float val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueString( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, const char * val );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_GameNetConnectionStatusChanged( IGameNetworkingUtils* self, FnGameNetConnectionStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_GameNetAuthenticationStatusChanged( IGameNetworkingUtils* self, FnGameNetAuthenticationStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged( IGameNetworkingUtils* self, FnSteamRelayNetworkStatusChanged fnCallback );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValue( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj, EGameNetworkingConfigDataType eDataType, const void * pArg );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValueStruct( IGameNetworkingUtils* self, const GameNetworkingConfigValue_t & opt, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj );
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingGetConfigValueResult SteamAPI_IGameNetworkingUtils_GetConfigValue( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj, EGameNetworkingConfigDataType * pOutDataType, void * pResult, size_t * cbResult );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_GetConfigValueInfo( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, const char ** pOutName, EGameNetworkingConfigDataType * pOutDataType, EGameNetworkingConfigScope * pOutScope, EGameNetworkingConfigValue * pOutNextValue );
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingConfigValue SteamAPI_IGameNetworkingUtils_GetFirstConfigValue( IGameNetworkingUtils* self );

// GameNetworkingIPAddr
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_Clear( GameNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsIPv6AllZeros( GameNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv6( GameNetworkingIPAddr* self, const uint8 * ipv6, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv4( GameNetworkingIPAddr* self, uint32 nIP, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsIPv4( GameNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE uint32 SteamAPI_GameNetworkingIPAddr_GetIPv4( GameNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv6LocalHost( GameNetworkingIPAddr* self, uint16 nPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsLocalHost( GameNetworkingIPAddr* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsEqualTo( GameNetworkingIPAddr* self, const GameNetworkingIPAddr & x );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr* self, char *buf, size_t cbBuf, bool bWithPort );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr* self, const char *pszStr );

// GameNetworkingIdentity
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_Clear( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsInvalid( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetSteamID( GameNetworkingIdentity* self, uint64_steamid steamID );
STEAMNETWORKINGSOCKETS_INTERFACE uint64_steamid SteamAPI_GameNetworkingIdentity_GetSteamID( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetSteamID64( GameNetworkingIdentity* self, uint64 steamID );
STEAMNETWORKINGSOCKETS_INTERFACE uint64 SteamAPI_GameNetworkingIdentity_GetSteamID64( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetIPAddr( GameNetworkingIdentity* self, const GameNetworkingIPAddr & addr );
STEAMNETWORKINGSOCKETS_INTERFACE const GameNetworkingIPAddr * SteamAPI_GameNetworkingIdentity_GetIPAddr( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetLocalHost( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsLocalHost( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_SetGenericString( GameNetworkingIdentity* self, const char * pszString );
STEAMNETWORKINGSOCKETS_INTERFACE const char * SteamAPI_GameNetworkingIdentity_GetGenericString( GameNetworkingIdentity* self );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_SetGenericBytes( GameNetworkingIdentity* self, const void * data, uint32 cbLen );
STEAMNETWORKINGSOCKETS_INTERFACE const uint8 * SteamAPI_GameNetworkingIdentity_GetGenericBytes( GameNetworkingIdentity* self, int & cbLen );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsEqualTo( GameNetworkingIdentity* self, const GameNetworkingIdentity & x );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_ToString( const GameNetworkingIdentity* self, char *buf, size_t cbBuf );
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_ParseString( GameNetworkingIdentity* self, size_t sizeofIdentity, const char *pszStr );

// GameNetworkingMessage_t
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingMessage_t_Release( GameNetworkingMessage_t* self );

// SteamDatagramHostedAddress
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_Clear( SteamDatagramHostedAddress* self );
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingPOPID SteamAPI_SteamDatagramHostedAddress_GetPopID( SteamDatagramHostedAddress* self );
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_SetDevAddress( SteamDatagramHostedAddress* self, uint32 nIP, uint16 nPort, GameNetworkingPOPID popid );
#endif

//
// Special flat functions to make it easier to work with custom signaling
//

typedef bool (*FGameNetworkingSocketsCustomSignaling_SendSignal)( void *ctx, HGameNetConnection hConn, const GameNetConnectionInfo_t &info, const void *pMsg, int cbMsg );
typedef void (*FGameNetworkingSocketsCustomSignaling_Release)( void *ctx );

/// Create an IGameNetworkingConnectionSignaling object from plain C primitives.
STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingConnectionSignaling *SteamAPI_IGameNetworkingSockets_CreateCustomSignaling(
	void *ctx, //< pointer to something useful you understand.  Will be passed to your callbacks.
	FGameNetworkingSocketsCustomSignaling_SendSignal fnSendSignal, //< Callback to send a signal.  See IGameNetworkingConnectionSignaling::SendSignal
	FGameNetworkingSocketsCustomSignaling_Release fnRelease //< callback to do any cleanup.  See IGameNetworkingConnectionSignaling::Release.  You can pass NULL if you don't need to do any cleanup.
);

typedef IGameNetworkingConnectionSignaling * (*FGameNetworkingCustomSignalingRecvContext_OnConnectRequest)( void *ctx, HGameNetConnection hConn, const GameNetworkingIdentity &identityPeer, int nLocalVirtualPort );
typedef void (*FGameNetworkingCustomSignalingRecvContext_SendRejectionSignal)( void *ctx, const GameNetworkingIdentity &identityPeer, const void *pMsg, int cbMsg );

/// Same as SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal, but using plain C primitives.
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal2(
	IGameNetworkingSockets* self, const void * pMsg, int cbMsg, //< Same as SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal
	void *ctx, //< pointer to something useful you understand.  Will be passed to your callbacks.
	FGameNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest, //< callback for sending a signal.  Required.  See IGameNetworkingSignalingRecvContext::OnConnectRequest
	FGameNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal //< callback when we wish to actively reject the connection.  Optional, pass NULL if you don't need this.  See IGameNetworkingSignalingRecvContext::SendRejectionSignal
);

#endif // STEAMNETWORKINGSOCKETS_FLAT

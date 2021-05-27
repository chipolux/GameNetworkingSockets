//====== Copyright Valve Corporation, All rights reserved. ====================

#include <steam/steamnetworkingsockets_flat.h>

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
#include <steam/steamdatagram_tickets.h>
#endif

#ifdef STEAMNETWORKINGSOCKETS_STANDALONELIB
#include <steam/steamnetworkingsockets.h>
#include <steam/steamnetworkingcustomsignaling.h>
#endif

//--- IGameNetworkingSockets-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingSockets *SteamAPI_GameNetworkingSockets_v009()
{
	return GameNetworkingSockets();
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketIP( IGameNetworkingSockets* self, const GameNetworkingIPAddr & localAddress, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->CreateListenSocketIP( localAddress,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectByIPAddress( IGameNetworkingSockets* self, const GameNetworkingIPAddr & address, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->ConnectByIPAddress( address,nOptions,pOptions );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketP2P( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->CreateListenSocketP2P( nLocalVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2P( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityRemote, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->ConnectP2P( identityRemote,nRemoteVirtualPort,nOptions,pOptions );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_AcceptConnection( IGameNetworkingSockets* self, HGameNetConnection hConn )
{
	return self->AcceptConnection( hConn );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseConnection( IGameNetworkingSockets* self, HGameNetConnection hPeer, int nReason, const char * pszDebug, bool bEnableLinger )
{
	return self->CloseConnection( hPeer,nReason,pszDebug,bEnableLinger );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseListenSocket( IGameNetworkingSockets* self, HSteamListenSocket hSocket )
{
	return self->CloseListenSocket( hSocket );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionUserData( IGameNetworkingSockets* self, HGameNetConnection hPeer, int64 nUserData )
{
	return self->SetConnectionUserData( hPeer,nUserData );
}
STEAMNETWORKINGSOCKETS_INTERFACE int64 SteamAPI_IGameNetworkingSockets_GetConnectionUserData( IGameNetworkingSockets* self, HGameNetConnection hPeer )
{
	return self->GetConnectionUserData( hPeer );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SetConnectionName( IGameNetworkingSockets* self, HGameNetConnection hPeer, const char * pszName )
{
	self->SetConnectionName( hPeer,pszName );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionName( IGameNetworkingSockets* self, HGameNetConnection hPeer, char * pszName, int nMaxLen )
{
	return self->GetConnectionName( hPeer,pszName,nMaxLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_SendMessageToConnection( IGameNetworkingSockets* self, HGameNetConnection hConn, const void * pData, uint32 cbData, int nSendFlags, int64 * pOutMessageNumber )
{
	return self->SendMessageToConnection( hConn,pData,cbData,nSendFlags,pOutMessageNumber );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SendMessages( IGameNetworkingSockets* self, int nMessages, GameNetworkingMessage_t *const * pMessages, int64 * pOutMessageNumberOrResult )
{
	self->SendMessages( nMessages,pMessages,pOutMessageNumberOrResult );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_FlushMessagesOnConnection( IGameNetworkingSockets* self, HGameNetConnection hConn )
{
	return self->FlushMessagesOnConnection( hConn );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnConnection( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetworkingMessage_t ** ppOutMessages, int nMaxMessages )
{
	return self->ReceiveMessagesOnConnection( hConn,ppOutMessages,nMaxMessages );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionInfo( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetConnectionInfo_t * pInfo )
{
	return self->GetConnectionInfo( hConn,pInfo );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetQuickConnectionStatus( IGameNetworkingSockets* self, HGameNetConnection hConn, GameNetworkingQuickConnectionStatus * pStats )
{
	return self->GetQuickConnectionStatus( hConn,pStats );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_GetDetailedConnectionStatus( IGameNetworkingSockets* self, HGameNetConnection hConn, char * pszBuf, int cbBuf )
{
	return self->GetDetailedConnectionStatus( hConn,pszBuf,cbBuf );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetListenSocketAddress( IGameNetworkingSockets* self, HSteamListenSocket hSocket, GameNetworkingIPAddr * address )
{
	return self->GetListenSocketAddress( hSocket,address );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CreateSocketPair( IGameNetworkingSockets* self, HGameNetConnection * pOutConnection1, HGameNetConnection * pOutConnection2, bool bUseNetworkLoopback, const GameNetworkingIdentity * pIdentity1, const GameNetworkingIdentity * pIdentity2 )
{
	return self->CreateSocketPair( pOutConnection1,pOutConnection2,bUseNetworkLoopback,pIdentity1,pIdentity2 );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetIdentity( IGameNetworkingSockets* self, GameNetworkingIdentity * pIdentity )
{
	return self->GetIdentity( pIdentity );
}
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingSockets_InitAuthentication( IGameNetworkingSockets* self )
{
	return self->InitAuthentication(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingSockets_GetAuthenticationStatus( IGameNetworkingSockets* self, GameNetAuthenticationStatus_t * pDetails )
{
	return self->GetAuthenticationStatus( pDetails );
}
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetPollGroup SteamAPI_IGameNetworkingSockets_CreatePollGroup( IGameNetworkingSockets* self )
{
	return self->CreatePollGroup(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_DestroyPollGroup( IGameNetworkingSockets* self, HGameNetPollGroup hPollGroup )
{
	return self->DestroyPollGroup( hPollGroup );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionPollGroup( IGameNetworkingSockets* self, HGameNetConnection hConn, HGameNetPollGroup hPollGroup )
{
	return self->SetConnectionPollGroup( hConn,hPollGroup );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnPollGroup( IGameNetworkingSockets* self, HGameNetPollGroup hPollGroup, GameNetworkingMessage_t ** ppOutMessages, int nMaxMessages )
{
	return self->ReceiveMessagesOnPollGroup( hPollGroup,ppOutMessages,nMaxMessages );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedRelayAuthTicket( IGameNetworkingSockets* self, const void * pvTicket, int cbTicket, SteamDatagramRelayAuthTicket * pOutParsedTicket )
{
	return self->ReceivedRelayAuthTicket( pvTicket,cbTicket,pOutParsedTicket );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_FindRelayAuthTicketForServer( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket * pOutParsedTicket )
{
	return self->FindRelayAuthTicketForServer( identityGameServer,nRemoteVirtualPort,pOutParsedTicket );
}
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectToHostedDedicatedServer( IGameNetworkingSockets* self, const GameNetworkingIdentity & identityTarget, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->ConnectToHostedDedicatedServer( identityTarget,nRemoteVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint16 SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPort( IGameNetworkingSockets* self )
{
	return self->GetHostedDedicatedServerPort(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingPOPID SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPOPID( IGameNetworkingSockets* self )
{
	return self->GetHostedDedicatedServerPOPID(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerAddress( IGameNetworkingSockets* self, SteamDatagramHostedAddress * pRouting )
{
	return self->GetHostedDedicatedServerAddress( pRouting );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateHostedDedicatedServerListenSocket( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->CreateHostedDedicatedServerListenSocket( nLocalVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetGameCoordinatorServerLogin( IGameNetworkingSockets* self, SteamDatagramGameCoordinatorServerLogin * pLoginInfo, int * pcbSignedBlob, void * pBlob )
{
	return self->GetGameCoordinatorServerLogin( pLoginInfo,pcbSignedBlob,pBlob );
}
STEAMNETWORKINGSOCKETS_INTERFACE HGameNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2PCustomSignaling( IGameNetworkingSockets* self, IGameNetworkingConnectionSignaling * pSignaling, const GameNetworkingIdentity * pPeerIdentity, int nRemoteVirtualPort, int nOptions, const GameNetworkingConfigValue_t * pOptions )
{
	return self->ConnectP2PCustomSignaling( pSignaling,pPeerIdentity,nRemoteVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal( IGameNetworkingSockets* self, const void * pMsg, int cbMsg, IGameNetworkingSignalingRecvContext * pContext )
{
	return self->ReceivedP2PCustomSignal( pMsg,cbMsg,pContext );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetCertificateRequest( IGameNetworkingSockets* self, int * pcbBlob, void * pBlob, GameNetworkingErrMsg & errMsg )
{
	return self->GetCertificateRequest( pcbBlob,pBlob,errMsg );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetCertificate( IGameNetworkingSockets* self, const void * pCertificate, int cbCertificate, GameNetworkingErrMsg & errMsg )
{
	return self->SetCertificate( pCertificate,cbCertificate,errMsg );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_RunCallbacks( IGameNetworkingSockets* self )
{
	self->RunCallbacks(  );
}

//--- IGameNetworkingUtils-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingUtils *SteamAPI_GameNetworkingUtils_v003()
{
	return GameNetworkingUtils();
}
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingMessage_t * SteamAPI_IGameNetworkingUtils_AllocateMessage( IGameNetworkingUtils* self, int cbAllocateBuffer )
{
	return self->AllocateMessage( cbAllocateBuffer );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_InitRelayNetworkAccess( IGameNetworkingUtils* self )
{
	self->InitRelayNetworkAccess(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingAvailability SteamAPI_IGameNetworkingUtils_GetRelayNetworkStatus( IGameNetworkingUtils* self, SteamRelayNetworkStatus_t * pDetails )
{
	return self->GetRelayNetworkStatus( pDetails );
}
STEAMNETWORKINGSOCKETS_INTERFACE float SteamAPI_IGameNetworkingUtils_GetLocalPingLocation( IGameNetworkingUtils* self, GameNetworkPingLocation_t & result )
{
	return self->GetLocalPingLocation( result );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeBetweenTwoLocations( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & location1, const GameNetworkPingLocation_t & location2 )
{
	return self->EstimatePingTimeBetweenTwoLocations( location1,location2 );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeFromLocalHost( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & remoteLocation )
{
	return self->EstimatePingTimeFromLocalHost( remoteLocation );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_ConvertPingLocationToString( IGameNetworkingUtils* self, const GameNetworkPingLocation_t & location, char * pszBuf, int cchBufSize )
{
	self->ConvertPingLocationToString( location,pszBuf,cchBufSize );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_ParsePingLocationString( IGameNetworkingUtils* self, const char * pszString, GameNetworkPingLocation_t & result )
{
	return self->ParsePingLocationString( pszString,result );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_CheckPingDataUpToDate( IGameNetworkingUtils* self, float flMaxAgeSeconds )
{
	return self->CheckPingDataUpToDate( flMaxAgeSeconds );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPingToDataCenter( IGameNetworkingUtils* self, GameNetworkingPOPID popID, GameNetworkingPOPID * pViaRelayPoP )
{
	return self->GetPingToDataCenter( popID,pViaRelayPoP );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetDirectPingToPOP( IGameNetworkingUtils* self, GameNetworkingPOPID popID )
{
	return self->GetDirectPingToPOP( popID );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPCount( IGameNetworkingUtils* self )
{
	return self->GetPOPCount(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPList( IGameNetworkingUtils* self, GameNetworkingPOPID * list, int nListSz )
{
	return self->GetPOPList( list,nListSz );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingMicroseconds SteamAPI_IGameNetworkingUtils_GetLocalTimestamp( IGameNetworkingUtils* self )
{
	return self->GetLocalTimestamp(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_SetDebugOutputFunction( IGameNetworkingUtils* self, EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc )
{
	self->SetDebugOutputFunction( eDetailLevel,pfnFunc );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueInt32( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, int32 val )
{
	return self->SetGlobalConfigValueInt32( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueFloat( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, float val )
{
	return self->SetGlobalConfigValueFloat( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueString( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, const char * val )
{
	return self->SetGlobalConfigValueString( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValuePtr( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, void * val )
{
	return self->SetGlobalConfigValuePtr( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueInt32( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, int32 val )
{
	return self->SetConnectionConfigValueInt32( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueFloat( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, float val )
{
	return self->SetConnectionConfigValueFloat( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueString( IGameNetworkingUtils* self, HGameNetConnection hConn, EGameNetworkingConfigValue eValue, const char * val )
{
	return self->SetConnectionConfigValueString( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_GameNetConnectionStatusChanged( IGameNetworkingUtils* self, FnGameNetConnectionStatusChanged fnCallback )
{
	return self->SetGlobalCallback_GameNetConnectionStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_GameNetAuthenticationStatusChanged( IGameNetworkingUtils* self, FnGameNetAuthenticationStatusChanged fnCallback )
{
	return self->SetGlobalCallback_GameNetAuthenticationStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged( IGameNetworkingUtils* self, FnSteamRelayNetworkStatusChanged fnCallback )
{
	return self->SetGlobalCallback_SteamRelayNetworkStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValue( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj, EGameNetworkingConfigDataType eDataType, const void * pArg )
{
	return self->SetConfigValue( eValue,eScopeType,scopeObj,eDataType,pArg );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValueStruct( IGameNetworkingUtils* self, const GameNetworkingConfigValue_t & opt, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj )
{
	return self->SetConfigValueStruct( opt,eScopeType,scopeObj );
}
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingGetConfigValueResult SteamAPI_IGameNetworkingUtils_GetConfigValue( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj, EGameNetworkingConfigDataType * pOutDataType, void * pResult, size_t * cbResult )
{
	return self->GetConfigValue( eValue,eScopeType,scopeObj,pOutDataType,pResult,cbResult );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_GetConfigValueInfo( IGameNetworkingUtils* self, EGameNetworkingConfigValue eValue, const char ** pOutName, EGameNetworkingConfigDataType * pOutDataType, EGameNetworkingConfigScope * pOutScope, EGameNetworkingConfigValue * pOutNextValue )
{
	return self->GetConfigValueInfo( eValue,pOutName,pOutDataType,pOutScope,pOutNextValue );
}
STEAMNETWORKINGSOCKETS_INTERFACE EGameNetworkingConfigValue SteamAPI_IGameNetworkingUtils_GetFirstConfigValue( IGameNetworkingUtils* self )
{
	return self->GetFirstConfigValue(  );
}

//--- GameNetworkingIPAddr-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_Clear( GameNetworkingIPAddr* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsIPv6AllZeros( GameNetworkingIPAddr* self )
{
	return self->IsIPv6AllZeros(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv6( GameNetworkingIPAddr* self, const uint8 * ipv6, uint16 nPort )
{
	self->SetIPv6( ipv6,nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv4( GameNetworkingIPAddr* self, uint32 nIP, uint16 nPort )
{
	self->SetIPv4( nIP,nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsIPv4( GameNetworkingIPAddr* self )
{
	return self->IsIPv4(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint32 SteamAPI_GameNetworkingIPAddr_GetIPv4( GameNetworkingIPAddr* self )
{
	return self->GetIPv4(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_SetIPv6LocalHost( GameNetworkingIPAddr* self, uint16 nPort )
{
	self->SetIPv6LocalHost( nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsLocalHost( GameNetworkingIPAddr* self )
{
	return self->IsLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_IsEqualTo( GameNetworkingIPAddr* self, const GameNetworkingIPAddr & x )
{
	return self->operator==( x );
}

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr* self, char *buf, size_t cbBuf, bool bWithPort )
{
	GameNetworkingIPAddr_ToString( self, buf, cbBuf, bWithPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr* self, const char *pszStr )
{
	return GameNetworkingIPAddr_ParseString( self, pszStr );
}

//--- GameNetworkingIdentity-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_Clear( GameNetworkingIdentity* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsInvalid( GameNetworkingIdentity* self )
{
	return self->IsInvalid(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetSteamID( GameNetworkingIdentity* self, uint64_steamid steamID )
{
	self->SetSteamID( CSteamID(steamID) );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint64_steamid SteamAPI_GameNetworkingIdentity_GetSteamID( GameNetworkingIdentity* self )
{
	return (self->GetSteamID(  )).ConvertToUint64();
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetSteamID64( GameNetworkingIdentity* self, uint64 steamID )
{
	self->SetSteamID64( steamID );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint64 SteamAPI_GameNetworkingIdentity_GetSteamID64( GameNetworkingIdentity* self )
{
	return self->GetSteamID64(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetIPAddr( GameNetworkingIdentity* self, const GameNetworkingIPAddr & addr )
{
	self->SetIPAddr( addr );
}
STEAMNETWORKINGSOCKETS_INTERFACE const GameNetworkingIPAddr * SteamAPI_GameNetworkingIdentity_GetIPAddr( GameNetworkingIdentity* self )
{
	return self->GetIPAddr(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_SetLocalHost( GameNetworkingIdentity* self )
{
	self->SetLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsLocalHost( GameNetworkingIdentity* self )
{
	return self->IsLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_SetGenericString( GameNetworkingIdentity* self, const char * pszString )
{
	return self->SetGenericString( pszString );
}
STEAMNETWORKINGSOCKETS_INTERFACE const char * SteamAPI_GameNetworkingIdentity_GetGenericString( GameNetworkingIdentity* self )
{
	return self->GetGenericString(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_SetGenericBytes( GameNetworkingIdentity* self, const void * data, uint32 cbLen )
{
	return self->SetGenericBytes( data,cbLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE const uint8 * SteamAPI_GameNetworkingIdentity_GetGenericBytes( GameNetworkingIdentity* self, int & cbLen )
{
	return self->GetGenericBytes( cbLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_IsEqualTo( GameNetworkingIdentity* self, const GameNetworkingIdentity & x )
{
	return self->operator==( x );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingIdentity_ToString( const GameNetworkingIdentity* self, char *buf, size_t cbBuf )
{
	GameNetworkingIdentity_ToString( self, buf, cbBuf );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_GameNetworkingIdentity_ParseString( GameNetworkingIdentity* self, size_t sizeofIdentity, const char *pszStr )
{
	return GameNetworkingIdentity_ParseString( self, sizeofIdentity, pszStr );
}

//--- GameNetworkingMessage_t-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_GameNetworkingMessage_t_Release( GameNetworkingMessage_t* self )
{
	self->Release(  );
}

//--- SteamDatagramHostedAddress-------------------------

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_Clear( SteamDatagramHostedAddress* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE GameNetworkingPOPID SteamAPI_SteamDatagramHostedAddress_GetPopID( SteamDatagramHostedAddress* self )
{
	return self->GetPopID(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_SetDevAddress( SteamDatagramHostedAddress* self, uint32 nIP, uint16 nPort, GameNetworkingPOPID popid )
{
	self->SetDevAddress( nIP,nPort,popid );
}

#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

//--- Special flat functions for custom signaling -------------------------

STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingConnectionSignaling *SteamAPI_IGameNetworkingSockets_CreateCustomSignaling(
	void *ctx, // pointer to something useful you understand.  Will be passed to your callbacks.
	FGameNetworkingSocketsCustomSignaling_SendSignal fnSendSignal, //< Callback to send a signal.  See IGameNetworkingConnectionSignaling::SendSignal
	FGameNetworkingSocketsCustomSignaling_Release fnRelease //< callback to do any cleanup.  See IGameNetworkingConnectionSignaling::Release.  You can pass NULL if you don't need to do any cleanup.
) {

	struct FlatSignalingAdapter final : IGameNetworkingConnectionSignaling
	{
		void *const m_ctx;
		FGameNetworkingSocketsCustomSignaling_SendSignal const m_fnSendSignal;
		FGameNetworkingSocketsCustomSignaling_Release const m_fnRelease;

		FlatSignalingAdapter(
			void *ctx,
			FGameNetworkingSocketsCustomSignaling_SendSignal fnSendSignal,
			FGameNetworkingSocketsCustomSignaling_Release fnRelease
		) : m_ctx ( ctx ), m_fnSendSignal( fnSendSignal ), m_fnRelease( fnRelease )
		{
		}

		virtual bool SendSignal( HGameNetConnection hConn, const GameNetConnectionInfo_t &info, const void *pMsg, int cbMsg ) override
		{
			return (*m_fnSendSignal)( m_ctx, hConn, info, pMsg, cbMsg );
		}
		virtual void Release() override
		{

			// Invoke app cleanup callback, if any
			if ( m_fnRelease )
				(*m_fnRelease)( m_ctx );

			// Self destruct
			delete this;
		}
	};

	return new FlatSignalingAdapter( ctx, fnSendSignal, fnRelease );
}

STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal2(
	IGameNetworkingSockets* self, const void * pMsg, int cbMsg,
	void *ctx,
	FGameNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest,
	FGameNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal
) {
	struct FlatRecvContextAdapter final : IGameNetworkingSignalingRecvContext
	{
		void *const m_ctx;
		FGameNetworkingCustomSignalingRecvContext_OnConnectRequest const m_fnOnConnectRequest;
		FGameNetworkingCustomSignalingRecvContext_SendRejectionSignal const m_fnSendRejectionSignal;

		FlatRecvContextAdapter(
			void *ctx,
			FGameNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest,
			FGameNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal
		) : m_ctx ( ctx ), m_fnOnConnectRequest( fnOnConnectRequest ), m_fnSendRejectionSignal( fnSendRejectionSignal )
		{
		}

		virtual IGameNetworkingConnectionSignaling *OnConnectRequest( HGameNetConnection hConn, const GameNetworkingIdentity &identityPeer, int nLocalVirtualPort ) override
		{
			return (*m_fnOnConnectRequest)( m_ctx, hConn, identityPeer, nLocalVirtualPort );
		}

		virtual void SendRejectionSignal( const GameNetworkingIdentity &identityPeer, const void *pMsg, int cbMsg ) override
		{
			if ( m_fnSendRejectionSignal )
				(*m_fnSendRejectionSignal)( m_ctx, identityPeer, pMsg, cbMsg );
		}
	};

	FlatRecvContextAdapter adapter( ctx, fnOnConnectRequest, fnSendRejectionSignal );
	return self->ReceivedP2PCustomSignal( pMsg, cbMsg, &adapter );
}


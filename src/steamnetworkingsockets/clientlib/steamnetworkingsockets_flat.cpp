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
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketIP( IGameNetworkingSockets* self, const SteamNetworkingIPAddr & localAddress, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->CreateListenSocketIP( localAddress,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectByIPAddress( IGameNetworkingSockets* self, const SteamNetworkingIPAddr & address, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->ConnectByIPAddress( address,nOptions,pOptions );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateListenSocketP2P( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->CreateListenSocketP2P( nLocalVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2P( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityRemote, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->ConnectP2P( identityRemote,nRemoteVirtualPort,nOptions,pOptions );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_AcceptConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn )
{
	return self->AcceptConnection( hConn );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseConnection( IGameNetworkingSockets* self, HSteamNetConnection hPeer, int nReason, const char * pszDebug, bool bEnableLinger )
{
	return self->CloseConnection( hPeer,nReason,pszDebug,bEnableLinger );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CloseListenSocket( IGameNetworkingSockets* self, HSteamListenSocket hSocket )
{
	return self->CloseListenSocket( hSocket );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionUserData( IGameNetworkingSockets* self, HSteamNetConnection hPeer, int64 nUserData )
{
	return self->SetConnectionUserData( hPeer,nUserData );
}
STEAMNETWORKINGSOCKETS_INTERFACE int64 SteamAPI_IGameNetworkingSockets_GetConnectionUserData( IGameNetworkingSockets* self, HSteamNetConnection hPeer )
{
	return self->GetConnectionUserData( hPeer );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SetConnectionName( IGameNetworkingSockets* self, HSteamNetConnection hPeer, const char * pszName )
{
	self->SetConnectionName( hPeer,pszName );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionName( IGameNetworkingSockets* self, HSteamNetConnection hPeer, char * pszName, int nMaxLen )
{
	return self->GetConnectionName( hPeer,pszName,nMaxLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_SendMessageToConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn, const void * pData, uint32 cbData, int nSendFlags, int64 * pOutMessageNumber )
{
	return self->SendMessageToConnection( hConn,pData,cbData,nSendFlags,pOutMessageNumber );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingSockets_SendMessages( IGameNetworkingSockets* self, int nMessages, SteamNetworkingMessage_t *const * pMessages, int64 * pOutMessageNumberOrResult )
{
	self->SendMessages( nMessages,pMessages,pOutMessageNumberOrResult );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_FlushMessagesOnConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn )
{
	return self->FlushMessagesOnConnection( hConn );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnConnection( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages )
{
	return self->ReceiveMessagesOnConnection( hConn,ppOutMessages,nMaxMessages );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetConnectionInfo( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetConnectionInfo_t * pInfo )
{
	return self->GetConnectionInfo( hConn,pInfo );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetQuickConnectionStatus( IGameNetworkingSockets* self, HSteamNetConnection hConn, SteamNetworkingQuickConnectionStatus * pStats )
{
	return self->GetQuickConnectionStatus( hConn,pStats );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_GetDetailedConnectionStatus( IGameNetworkingSockets* self, HSteamNetConnection hConn, char * pszBuf, int cbBuf )
{
	return self->GetDetailedConnectionStatus( hConn,pszBuf,cbBuf );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetListenSocketAddress( IGameNetworkingSockets* self, HSteamListenSocket hSocket, SteamNetworkingIPAddr * address )
{
	return self->GetListenSocketAddress( hSocket,address );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_CreateSocketPair( IGameNetworkingSockets* self, HSteamNetConnection * pOutConnection1, HSteamNetConnection * pOutConnection2, bool bUseNetworkLoopback, const SteamNetworkingIdentity * pIdentity1, const SteamNetworkingIdentity * pIdentity2 )
{
	return self->CreateSocketPair( pOutConnection1,pOutConnection2,bUseNetworkLoopback,pIdentity1,pIdentity2 );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetIdentity( IGameNetworkingSockets* self, SteamNetworkingIdentity * pIdentity )
{
	return self->GetIdentity( pIdentity );
}
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingSockets_InitAuthentication( IGameNetworkingSockets* self )
{
	return self->InitAuthentication(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingSockets_GetAuthenticationStatus( IGameNetworkingSockets* self, SteamNetAuthenticationStatus_t * pDetails )
{
	return self->GetAuthenticationStatus( pDetails );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetPollGroup SteamAPI_IGameNetworkingSockets_CreatePollGroup( IGameNetworkingSockets* self )
{
	return self->CreatePollGroup(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_DestroyPollGroup( IGameNetworkingSockets* self, HSteamNetPollGroup hPollGroup )
{
	return self->DestroyPollGroup( hPollGroup );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetConnectionPollGroup( IGameNetworkingSockets* self, HSteamNetConnection hConn, HSteamNetPollGroup hPollGroup )
{
	return self->SetConnectionPollGroup( hConn,hPollGroup );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_ReceiveMessagesOnPollGroup( IGameNetworkingSockets* self, HSteamNetPollGroup hPollGroup, SteamNetworkingMessage_t ** ppOutMessages, int nMaxMessages )
{
	return self->ReceiveMessagesOnPollGroup( hPollGroup,ppOutMessages,nMaxMessages );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedRelayAuthTicket( IGameNetworkingSockets* self, const void * pvTicket, int cbTicket, SteamDatagramRelayAuthTicket * pOutParsedTicket )
{
	return self->ReceivedRelayAuthTicket( pvTicket,cbTicket,pOutParsedTicket );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingSockets_FindRelayAuthTicketForServer( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityGameServer, int nRemoteVirtualPort, SteamDatagramRelayAuthTicket * pOutParsedTicket )
{
	return self->FindRelayAuthTicketForServer( identityGameServer,nRemoteVirtualPort,pOutParsedTicket );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectToHostedDedicatedServer( IGameNetworkingSockets* self, const SteamNetworkingIdentity & identityTarget, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->ConnectToHostedDedicatedServer( identityTarget,nRemoteVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint16 SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPort( IGameNetworkingSockets* self )
{
	return self->GetHostedDedicatedServerPort(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingPOPID SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerPOPID( IGameNetworkingSockets* self )
{
	return self->GetHostedDedicatedServerPOPID(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetHostedDedicatedServerAddress( IGameNetworkingSockets* self, SteamDatagramHostedAddress * pRouting )
{
	return self->GetHostedDedicatedServerAddress( pRouting );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamListenSocket SteamAPI_IGameNetworkingSockets_CreateHostedDedicatedServerListenSocket( IGameNetworkingSockets* self, int nLocalVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->CreateHostedDedicatedServerListenSocket( nLocalVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE EResult SteamAPI_IGameNetworkingSockets_GetGameCoordinatorServerLogin( IGameNetworkingSockets* self, SteamDatagramGameCoordinatorServerLogin * pLoginInfo, int * pcbSignedBlob, void * pBlob )
{
	return self->GetGameCoordinatorServerLogin( pLoginInfo,pcbSignedBlob,pBlob );
}
STEAMNETWORKINGSOCKETS_INTERFACE HSteamNetConnection SteamAPI_IGameNetworkingSockets_ConnectP2PCustomSignaling( IGameNetworkingSockets* self, ISteamNetworkingConnectionSignaling * pSignaling, const SteamNetworkingIdentity * pPeerIdentity, int nRemoteVirtualPort, int nOptions, const SteamNetworkingConfigValue_t * pOptions )
{
	return self->ConnectP2PCustomSignaling( pSignaling,pPeerIdentity,nRemoteVirtualPort,nOptions,pOptions );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_ReceivedP2PCustomSignal( IGameNetworkingSockets* self, const void * pMsg, int cbMsg, ISteamNetworkingSignalingRecvContext * pContext )
{
	return self->ReceivedP2PCustomSignal( pMsg,cbMsg,pContext );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_GetCertificateRequest( IGameNetworkingSockets* self, int * pcbBlob, void * pBlob, SteamNetworkingErrMsg & errMsg )
{
	return self->GetCertificateRequest( pcbBlob,pBlob,errMsg );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingSockets_SetCertificate( IGameNetworkingSockets* self, const void * pCertificate, int cbCertificate, SteamNetworkingErrMsg & errMsg )
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
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingMessage_t * SteamAPI_IGameNetworkingUtils_AllocateMessage( IGameNetworkingUtils* self, int cbAllocateBuffer )
{
	return self->AllocateMessage( cbAllocateBuffer );
}
#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_InitRelayNetworkAccess( IGameNetworkingUtils* self )
{
	self->InitRelayNetworkAccess(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingAvailability SteamAPI_IGameNetworkingUtils_GetRelayNetworkStatus( IGameNetworkingUtils* self, SteamRelayNetworkStatus_t * pDetails )
{
	return self->GetRelayNetworkStatus( pDetails );
}
STEAMNETWORKINGSOCKETS_INTERFACE float SteamAPI_IGameNetworkingUtils_GetLocalPingLocation( IGameNetworkingUtils* self, SteamNetworkPingLocation_t & result )
{
	return self->GetLocalPingLocation( result );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeBetweenTwoLocations( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & location1, const SteamNetworkPingLocation_t & location2 )
{
	return self->EstimatePingTimeBetweenTwoLocations( location1,location2 );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_EstimatePingTimeFromLocalHost( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & remoteLocation )
{
	return self->EstimatePingTimeFromLocalHost( remoteLocation );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_ConvertPingLocationToString( IGameNetworkingUtils* self, const SteamNetworkPingLocation_t & location, char * pszBuf, int cchBufSize )
{
	self->ConvertPingLocationToString( location,pszBuf,cchBufSize );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_ParsePingLocationString( IGameNetworkingUtils* self, const char * pszString, SteamNetworkPingLocation_t & result )
{
	return self->ParsePingLocationString( pszString,result );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_CheckPingDataUpToDate( IGameNetworkingUtils* self, float flMaxAgeSeconds )
{
	return self->CheckPingDataUpToDate( flMaxAgeSeconds );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPingToDataCenter( IGameNetworkingUtils* self, SteamNetworkingPOPID popID, SteamNetworkingPOPID * pViaRelayPoP )
{
	return self->GetPingToDataCenter( popID,pViaRelayPoP );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetDirectPingToPOP( IGameNetworkingUtils* self, SteamNetworkingPOPID popID )
{
	return self->GetDirectPingToPOP( popID );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPCount( IGameNetworkingUtils* self )
{
	return self->GetPOPCount(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE int SteamAPI_IGameNetworkingUtils_GetPOPList( IGameNetworkingUtils* self, SteamNetworkingPOPID * list, int nListSz )
{
	return self->GetPOPList( list,nListSz );
}
#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingMicroseconds SteamAPI_IGameNetworkingUtils_GetLocalTimestamp( IGameNetworkingUtils* self )
{
	return self->GetLocalTimestamp(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_IGameNetworkingUtils_SetDebugOutputFunction( IGameNetworkingUtils* self, EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc )
{
	self->SetDebugOutputFunction( eDetailLevel,pfnFunc );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueInt32( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, int32 val )
{
	return self->SetGlobalConfigValueInt32( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueFloat( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, float val )
{
	return self->SetGlobalConfigValueFloat( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValueString( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, const char * val )
{
	return self->SetGlobalConfigValueString( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalConfigValuePtr( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, void * val )
{
	return self->SetGlobalConfigValuePtr( eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueInt32( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, int32 val )
{
	return self->SetConnectionConfigValueInt32( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueFloat( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, float val )
{
	return self->SetConnectionConfigValueFloat( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConnectionConfigValueString( IGameNetworkingUtils* self, HSteamNetConnection hConn, ESteamNetworkingConfigValue eValue, const char * val )
{
	return self->SetConnectionConfigValueString( hConn,eValue,val );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamNetConnectionStatusChanged( IGameNetworkingUtils* self, FnSteamNetConnectionStatusChanged fnCallback )
{
	return self->SetGlobalCallback_SteamNetConnectionStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamNetAuthenticationStatusChanged( IGameNetworkingUtils* self, FnSteamNetAuthenticationStatusChanged fnCallback )
{
	return self->SetGlobalCallback_SteamNetAuthenticationStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetGlobalCallback_SteamRelayNetworkStatusChanged( IGameNetworkingUtils* self, FnSteamRelayNetworkStatusChanged fnCallback )
{
	return self->SetGlobalCallback_SteamRelayNetworkStatusChanged( fnCallback );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValue( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType eDataType, const void * pArg )
{
	return self->SetConfigValue( eValue,eScopeType,scopeObj,eDataType,pArg );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_SetConfigValueStruct( IGameNetworkingUtils* self, const SteamNetworkingConfigValue_t & opt, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj )
{
	return self->SetConfigValueStruct( opt,eScopeType,scopeObj );
}
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingGetConfigValueResult SteamAPI_IGameNetworkingUtils_GetConfigValue( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, ESteamNetworkingConfigScope eScopeType, intptr_t scopeObj, ESteamNetworkingConfigDataType * pOutDataType, void * pResult, size_t * cbResult )
{
	return self->GetConfigValue( eValue,eScopeType,scopeObj,pOutDataType,pResult,cbResult );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_IGameNetworkingUtils_GetConfigValueInfo( IGameNetworkingUtils* self, ESteamNetworkingConfigValue eValue, const char ** pOutName, ESteamNetworkingConfigDataType * pOutDataType, ESteamNetworkingConfigScope * pOutScope, ESteamNetworkingConfigValue * pOutNextValue )
{
	return self->GetConfigValueInfo( eValue,pOutName,pOutDataType,pOutScope,pOutNextValue );
}
STEAMNETWORKINGSOCKETS_INTERFACE ESteamNetworkingConfigValue SteamAPI_IGameNetworkingUtils_GetFirstConfigValue( IGameNetworkingUtils* self )
{
	return self->GetFirstConfigValue(  );
}

//--- SteamNetworkingIPAddr-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_Clear( SteamNetworkingIPAddr* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsIPv6AllZeros( SteamNetworkingIPAddr* self )
{
	return self->IsIPv6AllZeros(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv6( SteamNetworkingIPAddr* self, const uint8 * ipv6, uint16 nPort )
{
	self->SetIPv6( ipv6,nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv4( SteamNetworkingIPAddr* self, uint32 nIP, uint16 nPort )
{
	self->SetIPv4( nIP,nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsIPv4( SteamNetworkingIPAddr* self )
{
	return self->IsIPv4(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint32 SteamAPI_SteamNetworkingIPAddr_GetIPv4( SteamNetworkingIPAddr* self )
{
	return self->GetIPv4(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_SetIPv6LocalHost( SteamNetworkingIPAddr* self, uint16 nPort )
{
	self->SetIPv6LocalHost( nPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsLocalHost( SteamNetworkingIPAddr* self )
{
	return self->IsLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_IsEqualTo( SteamNetworkingIPAddr* self, const SteamNetworkingIPAddr & x )
{
	return self->operator==( x );
}

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIPAddr_ToString( const SteamNetworkingIPAddr* self, char *buf, size_t cbBuf, bool bWithPort )
{
	SteamNetworkingIPAddr_ToString( self, buf, cbBuf, bWithPort );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIPAddr_ParseString( SteamNetworkingIPAddr* self, const char *pszStr )
{
	return SteamNetworkingIPAddr_ParseString( self, pszStr );
}

//--- SteamNetworkingIdentity-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_Clear( SteamNetworkingIdentity* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsInvalid( SteamNetworkingIdentity* self )
{
	return self->IsInvalid(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetSteamID( SteamNetworkingIdentity* self, uint64_steamid steamID )
{
	self->SetSteamID( CSteamID(steamID) );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint64_steamid SteamAPI_SteamNetworkingIdentity_GetSteamID( SteamNetworkingIdentity* self )
{
	return (self->GetSteamID(  )).ConvertToUint64();
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetSteamID64( SteamNetworkingIdentity* self, uint64 steamID )
{
	self->SetSteamID64( steamID );
}
STEAMNETWORKINGSOCKETS_INTERFACE uint64 SteamAPI_SteamNetworkingIdentity_GetSteamID64( SteamNetworkingIdentity* self )
{
	return self->GetSteamID64(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetIPAddr( SteamNetworkingIdentity* self, const SteamNetworkingIPAddr & addr )
{
	self->SetIPAddr( addr );
}
STEAMNETWORKINGSOCKETS_INTERFACE const SteamNetworkingIPAddr * SteamAPI_SteamNetworkingIdentity_GetIPAddr( SteamNetworkingIdentity* self )
{
	return self->GetIPAddr(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_SetLocalHost( SteamNetworkingIdentity* self )
{
	self->SetLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsLocalHost( SteamNetworkingIdentity* self )
{
	return self->IsLocalHost(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_SetGenericString( SteamNetworkingIdentity* self, const char * pszString )
{
	return self->SetGenericString( pszString );
}
STEAMNETWORKINGSOCKETS_INTERFACE const char * SteamAPI_SteamNetworkingIdentity_GetGenericString( SteamNetworkingIdentity* self )
{
	return self->GetGenericString(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_SetGenericBytes( SteamNetworkingIdentity* self, const void * data, uint32 cbLen )
{
	return self->SetGenericBytes( data,cbLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE const uint8 * SteamAPI_SteamNetworkingIdentity_GetGenericBytes( SteamNetworkingIdentity* self, int & cbLen )
{
	return self->GetGenericBytes( cbLen );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_IsEqualTo( SteamNetworkingIdentity* self, const SteamNetworkingIdentity & x )
{
	return self->operator==( x );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingIdentity_ToString( const SteamNetworkingIdentity* self, char *buf, size_t cbBuf )
{
	SteamNetworkingIdentity_ToString( self, buf, cbBuf );
}
STEAMNETWORKINGSOCKETS_INTERFACE bool SteamAPI_SteamNetworkingIdentity_ParseString( SteamNetworkingIdentity* self, size_t sizeofIdentity, const char *pszStr )
{
	return SteamNetworkingIdentity_ParseString( self, sizeofIdentity, pszStr );
}

//--- SteamNetworkingMessage_t-------------------------

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamNetworkingMessage_t_Release( SteamNetworkingMessage_t* self )
{
	self->Release(  );
}

//--- SteamDatagramHostedAddress-------------------------

#ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_Clear( SteamDatagramHostedAddress* self )
{
	self->Clear(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE SteamNetworkingPOPID SteamAPI_SteamDatagramHostedAddress_GetPopID( SteamDatagramHostedAddress* self )
{
	return self->GetPopID(  );
}
STEAMNETWORKINGSOCKETS_INTERFACE void SteamAPI_SteamDatagramHostedAddress_SetDevAddress( SteamDatagramHostedAddress* self, uint32 nIP, uint16 nPort, SteamNetworkingPOPID popid )
{
	self->SetDevAddress( nIP,nPort,popid );
}

#endif // #ifdef STEAMNETWORKINGSOCKETS_ENABLE_SDR

//--- Special flat functions for custom signaling -------------------------

STEAMNETWORKINGSOCKETS_INTERFACE ISteamNetworkingConnectionSignaling *SteamAPI_IGameNetworkingSockets_CreateCustomSignaling(
	void *ctx, // pointer to something useful you understand.  Will be passed to your callbacks.
	FGameNetworkingSocketsCustomSignaling_SendSignal fnSendSignal, //< Callback to send a signal.  See ISteamNetworkingConnectionSignaling::SendSignal
	FGameNetworkingSocketsCustomSignaling_Release fnRelease //< callback to do any cleanup.  See ISteamNetworkingConnectionSignaling::Release.  You can pass NULL if you don't need to do any cleanup.
) {

	struct FlatSignalingAdapter final : ISteamNetworkingConnectionSignaling
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

		virtual bool SendSignal( HSteamNetConnection hConn, const SteamNetConnectionInfo_t &info, const void *pMsg, int cbMsg ) override
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
	FSteamNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest,
	FSteamNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal
) {
	struct FlatRecvContextAdapter final : ISteamNetworkingSignalingRecvContext
	{
		void *const m_ctx;
		FSteamNetworkingCustomSignalingRecvContext_OnConnectRequest const m_fnOnConnectRequest;
		FSteamNetworkingCustomSignalingRecvContext_SendRejectionSignal const m_fnSendRejectionSignal;

		FlatRecvContextAdapter(
			void *ctx,
			FSteamNetworkingCustomSignalingRecvContext_OnConnectRequest fnOnConnectRequest,
			FSteamNetworkingCustomSignalingRecvContext_SendRejectionSignal fnSendRejectionSignal
		) : m_ctx ( ctx ), m_fnOnConnectRequest( fnOnConnectRequest ), m_fnSendRejectionSignal( fnSendRejectionSignal )
		{
		}

		virtual ISteamNetworkingConnectionSignaling *OnConnectRequest( HSteamNetConnection hConn, const SteamNetworkingIdentity &identityPeer, int nLocalVirtualPort ) override
		{
			return (*m_fnOnConnectRequest)( m_ctx, hConn, identityPeer, nLocalVirtualPort );
		}

		virtual void SendRejectionSignal( const SteamNetworkingIdentity &identityPeer, const void *pMsg, int cbMsg ) override
		{
			if ( m_fnSendRejectionSignal )
				(*m_fnSendRejectionSignal)( m_ctx, identityPeer, pMsg, cbMsg );
		}
	};

	FlatRecvContextAdapter adapter( ctx, fnOnConnectRequest, fnSendRejectionSignal );
	return self->ReceivedP2PCustomSignal( pMsg, cbMsg, &adapter );
}


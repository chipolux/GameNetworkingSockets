// Misc stuff used in the tests

#include "test_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <chrono>
#include <thread>

#include <gns/gamenetworkingsockets.h>
#include <gns/igamenetworkingutils.h>

#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <gns/game_api.h>
#endif

static FILE *g_fpLog = nullptr;
static GameNetworkingMicroseconds g_logTimeZero;

static void DebugOutput( EGameNetworkingSocketsDebugOutputType eType, const char *pszMsg )
{
	GameNetworkingMicroseconds time = GameNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
	if ( g_fpLog )
		fprintf( g_fpLog, "%10.6f %s\n", time*1e-6, pszMsg );
	//if ( eType <= k_EGameNetworkingSocketsDebugOutputType_Msg )
	{
		printf( "%10.6f %s\n", time*1e-6, pszMsg );
		fflush(stdout);
	}
	if ( eType == k_EGameNetworkingSocketsDebugOutputType_Bug )
	{
		fflush(stdout);
		fflush(stderr);
		if ( g_fpLog )
			fflush( g_fpLog );

		// !KLUDGE! Our logging (which is done while we hold the lock)
		// is occasionally triggering this assert.  Just ignroe that one
		// error for now.
		// Yes, this is a kludge.
		if ( strstr( pszMsg, "GameNetworkingGlobalLock held for" ) )
			return;

		assert( !"TEST FAILED" );
	}
}

void TEST_Printf( const char *fmt, ... )
{
	char text[ 2048 ];
	va_list ap;
	va_start( ap, fmt );
	vsprintf( text, fmt, ap );
	va_end(ap);
	char *nl = strchr( text, '\0' ) - 1;
	if ( nl >= text && *nl == '\n' )
		*nl = '\0';
	DebugOutput( k_EGameNetworkingSocketsDebugOutputType_Msg, text );
}

void TEST_Fatal( const char *fmt, ... )
{
	fflush(stdout);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fflush(stderr);
	exit(1);
}

void TEST_Init( const GameNetworkingIdentity *pIdentity )
{
	g_fpLog = fopen( "log.txt", "wt" );
	g_logTimeZero = GameNetworkingUtils()->GetLocalTimestamp();

	GameNetworkingUtils()->SetDebugOutputFunction( k_EGameNetworkingSocketsDebugOutputType_Debug, DebugOutput );
	//GameNetworkingUtils()->SetDebugOutputFunction( k_EGameNetworkingSocketsDebugOutputType_Verbose, DebugOutput );
	//GameNetworkingUtils()->SetDebugOutputFunction( k_EGameNetworkingSocketsDebugOutputType_Msg, DebugOutput );

	GameNetworkingUtils()->SetGlobalConfigValueInt32( k_EGameNetworkingConfig_LogLevel_P2PRendezvous, k_EGameNetworkingSocketsDebugOutputType_Debug );

	#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
		SteamDatagramErrMsg errMsg;
		if ( !GameNetworkingSockets_Init( pIdentity, errMsg ) )
		{
			fprintf( stderr, "GameNetworkingSockets_Init failed.  %s", errMsg );
			exit(1);
		}
	#else
		//SteamAPI_Init();

		// Cannot specify custom identity
		assert( pIdentity == nullptr );

		SteamDatagramClient_SetAppID( 570 ); // Just set something, doesn't matter what
		//SteamDatagramClient_SetUniverse( k_EUniverseDev );

		SteamDatagramErrMsg errMsg;
		if ( !SteamDatagramClient_Init( true, errMsg ) )
		{
			fprintf( stderr, "SteamDatagramClient_Init failed.  %s", errMsg );
			exit(1);
		}
    #endif
}

void TEST_Kill()
{
	#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
		GameNetworkingSockets_Kill();
	#else
		SteamDatagramClient_Kill();
	#endif
}

void TEST_PumpCallbacks()
{
	GameNetworkingSockets()->RunCallbacks();
	std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
}


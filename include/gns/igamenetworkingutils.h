//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Purpose: misc networking utilities
//
//=============================================================================

#ifndef ISTEAMNETWORKINGUTILS
#define ISTEAMNETWORKINGUTILS
#pragma once

#include "gamenetworkingtypes.h"
#include "game_api_common.h"

struct SteamDatagramRelayAuthTicket;
struct SteamRelayNetworkStatus_t;

//-----------------------------------------------------------------------------
/// Misc networking utilities for checking the local networking environment
/// and estimating pings.
class IGameNetworkingUtils
{
public:
	//
	// Efficient message sending
	//

	/// Allocate and initialize a message object.  Usually the reason
	/// you call this is to pass it to IGameNetworkingSockets::SendMessages.
	/// The returned object will have all of the relevant fields cleared to zero.
	///
	/// Optionally you can also request that this system allocate space to
	/// hold the payload itself.  If cbAllocateBuffer is nonzero, the system
	/// will allocate memory to hold a payload of at least cbAllocateBuffer bytes.
	/// m_pData will point to the allocated buffer, m_cbSize will be set to the
	/// size, and m_pfnFreeData will be set to the proper function to free up
	/// the buffer.
	///
	/// If cbAllocateBuffer=0, then no buffer is allocated.  m_pData will be NULL,
	/// m_cbSize will be zero, and m_pfnFreeData will be NULL.  You will need to
	/// set each of these.
	virtual GameNetworkingMessage_t *AllocateMessage( int cbAllocateBuffer ) = 0;

	//
	// Access to Steam Datagram Relay (SDR) network
	//

	//
	// Initialization and status check
	//

	/// If you know that you are going to be using the relay network (for example,
	/// because you anticipate making P2P connections), call this to initialize the
	/// relay network.  If you do not call this, the initialization will
	/// be delayed until the first time you use a feature that requires access
	/// to the relay network, which will delay that first access.
	///
	/// You can also call this to force a retry if the previous attempt has failed.
	/// Performing any action that requires access to the relay network will also
	/// trigger a retry, and so calling this function is never strictly necessary,
	/// but it can be useful to call it a program launch time, if access to the
	/// relay network is anticipated.
	///
	/// Use GetRelayNetworkStatus or listen for SteamRelayNetworkStatus_t
	/// callbacks to know when initialization has completed.
	/// Typically initialization completes in a few seconds.
	///
	/// Note: dedicated servers hosted in known data centers do *not* need
	/// to call this, since they do not make routing decisions.  However, if
	/// the dedicated server will be using P2P functionality, it will act as
	/// a "client" and this should be called.
	inline void InitRelayNetworkAccess();

	/// Fetch current status of the relay network.
	///
	/// SteamRelayNetworkStatus_t is also a callback.  It will be triggered on
	/// both the user and gameserver interfaces any time the status changes, or
	/// ping measurement starts or stops.
	///
	/// SteamRelayNetworkStatus_t::m_eAvail is returned.  If you want
	/// more details, you can pass a non-NULL value.
	virtual EGameNetworkingAvailability GetRelayNetworkStatus( SteamRelayNetworkStatus_t *pDetails ) = 0;

	//
	// "Ping location" functions
	//
	// We use the ping times to the valve relays deployed worldwide to
	// generate a "marker" that describes the location of an Internet host.
	// Given two such markers, we can estimate the network latency between
	// two hosts, without sending any packets.  The estimate is based on the
	// optimal route that is found through the Valve network.  If you are
	// using the Valve network to carry the traffic, then this is precisely
	// the ping you want.  If you are not, then the ping time will probably
	// still be a reasonable estimate.
	//
	// This is extremely useful to select peers for matchmaking!
	//
	// The markers can also be converted to a string, so they can be transmitted.
	// We have a separate library you can use on your app's matchmaking/coordinating
	// server to manipulate these objects.  (See gamedatagram_gamecoordinator.h)

	/// Return location info for the current host.  Returns the approximate
	/// age of the data, in seconds, or -1 if no data is available.
	///
	/// It takes a few seconds to initialize access to the relay network.  If
	/// you call this very soon after calling InitRelayNetworkAccess,
	/// the data may not be available yet.
	///
	/// This always return the most up-to-date information we have available
	/// right now, even if we are in the middle of re-calculating ping times.
	virtual float GetLocalPingLocation( GameNetworkPingLocation_t &result ) = 0;

	/// Estimate the round-trip latency between two arbitrary locations, in
	/// milliseconds.  This is a conservative estimate, based on routing through
	/// the relay network.  For most basic relayed connections, this ping time
	/// will be pretty accurate, since it will be based on the route likely to
	/// be actually used.
	///
	/// If a direct IP route is used (perhaps via NAT traversal), then the route
	/// will be different, and the ping time might be better.  Or it might actually
	/// be a bit worse!  Standard IP routing is frequently suboptimal!
	///
	/// But even in this case, the estimate obtained using this method is a
	/// reasonable upper bound on the ping time.  (Also it has the advantage
	/// of returning immediately and not sending any packets.)
	///
	/// In a few cases we might not able to estimate the route.  In this case
	/// a negative value is returned.  k_nGameNetworkingPing_Failed means
	/// the reason was because of some networking difficulty.  (Failure to
	/// ping, etc)  k_nGameNetworkingPing_Unknown is returned if we cannot
	/// currently answer the question for some other reason.
	///
	/// Do you need to be able to do this from a backend/matchmaking server?
	/// You are looking for the "ticketgen" library.
	virtual int EstimatePingTimeBetweenTwoLocations( const GameNetworkPingLocation_t &location1, const GameNetworkPingLocation_t &location2 ) = 0;

	/// Same as EstimatePingTime, but assumes that one location is the local host.
	/// This is a bit faster, especially if you need to calculate a bunch of
	/// these in a loop to find the fastest one.
	///
	/// In rare cases this might return a slightly different estimate than combining
	/// GetLocalPingLocation with EstimatePingTimeBetweenTwoLocations.  That's because
	/// this function uses a slightly more complete set of information about what
	/// route would be taken.
	virtual int EstimatePingTimeFromLocalHost( const GameNetworkPingLocation_t &remoteLocation ) = 0;

	/// Convert a ping location into a text format suitable for sending over the wire.
	/// The format is a compact and human readable.  However, it is subject to change
	/// so please do not parse it yourself.  Your buffer must be at least
	/// k_cchMaxGameNetworkingPingLocationString bytes.
	virtual void ConvertPingLocationToString( const GameNetworkPingLocation_t &location, char *pszBuf, int cchBufSize ) = 0;

	/// Parse back GameNetworkPingLocation_t string.  Returns false if we couldn't understand
	/// the string.
	virtual bool ParsePingLocationString( const char *pszString, GameNetworkPingLocation_t &result ) = 0;

	/// Check if the ping data of sufficient recency is available, and if
	/// it's too old, start refreshing it.
	///
	/// Please only call this function when you *really* do need to force an
	/// immediate refresh of the data.  (For example, in response to a specific
	/// user input to refresh this information.)  Don't call it "just in case",
	/// before every connection, etc.  That will cause extra traffic to be sent
	/// for no benefit. The library will automatically refresh the information
	/// as needed.
	///
	/// Returns true if sufficiently recent data is already available.
	///
	/// Returns false if sufficiently recent data is not available.  In this
	/// case, ping measurement is initiated, if it is not already active.
	/// (You cannot restart a measurement already in progress.)
	///
	/// You can use GetRelayNetworkStatus or listen for SteamRelayNetworkStatus_t
	/// to know when ping measurement completes.
	virtual bool CheckPingDataUpToDate( float flMaxAgeSeconds ) = 0;

	//
	// List of Valve data centers, and ping times to them.  This might
	// be useful to you if you are use our hosting, or just need to measure
	// latency to a cloud data center where we are running relays.
	//

	/// Fetch ping time of best available relayed route from this host to
	/// the specified data center.
	virtual int GetPingToDataCenter( GameNetworkingPOPID popID, GameNetworkingPOPID *pViaRelayPoP ) = 0;

	/// Get *direct* ping time to the relays at the data center.
	virtual int GetDirectPingToPOP( GameNetworkingPOPID popID ) = 0;

	/// Get number of network points of presence in the config
	virtual int GetPOPCount() = 0;

	/// Get list of all POP IDs.  Returns the number of entries that were filled into
	/// your list.
	virtual int GetPOPList( GameNetworkingPOPID *list, int nListSz ) = 0;

	//
	// Misc
	//

	/// Fetch current timestamp.  This timer has the following properties:
	///
	/// - Monotonicity is guaranteed.
	/// - The initial value will be at least 24*3600*30*1e6, i.e. about
	///   30 days worth of microseconds.  In this way, the timestamp value of
	///   0 will always be at least "30 days ago".  Also, negative numbers
	///   will never be returned.
	/// - Wraparound / overflow is not a practical concern.
	///
	/// If you are running under the debugger and stop the process, the clock
	/// might not advance the full wall clock time that has elapsed between
	/// calls.  If the process is not blocked from normal operation, the
	/// timestamp values will track wall clock time, even if you don't call
	/// the function frequently.
	///
	/// The value is only meaningful for this run of the process.  Don't compare
	/// it to values obtained on another computer, or other runs of the same process.
	virtual GameNetworkingMicroseconds GetLocalTimestamp() = 0;

	/// Set a function to receive network-related information that is useful for debugging.
	/// This can be very useful during development, but it can also be useful for troubleshooting
	/// problems with tech savvy end users.  If you have a console or other log that customers
	/// can examine, these log messages can often be helpful to troubleshoot network issues.
	/// (Especially any warning/error messages.)
	///
	/// The detail level indicates what message to invoke your callback on.  Lower numeric
	/// value means more important, and the value you pass is the lowest priority (highest
	/// numeric value) you wish to receive callbacks for.
	///
	/// The value here controls the detail level for most messages.  You can control the
	/// detail level for various subsystems (perhaps only for certain connections) by
	/// adjusting the configuration values k_EGameNetworkingConfig_LogLevel_Xxxxx.
	///
	/// Except when debugging, you should only use k_EGameNetworkingSocketsDebugOutputType_Msg
	/// or k_EGameNetworkingSocketsDebugOutputType_Warning.  For best performance, do NOT
	/// request a high detail level and then filter out messages in your callback.  This incurs
	/// all of the expense of formatting the messages, which are then discarded.  Setting a high
	/// priority value (low numeric value) here allows the library to avoid doing this work.
	///
	/// IMPORTANT: This may be called from a service thread, while we own a mutex, etc.
	/// Your output function must be threadsafe and fast!  Do not make any other
	/// Steamworks calls from within the handler.
	virtual void SetDebugOutputFunction( EGameNetworkingSocketsDebugOutputType eDetailLevel, FGameNetworkingSocketsDebugOutput pfnFunc ) = 0;

	//
	// Set and get configuration values, see EGameNetworkingConfigValue for individual descriptions.
	//

	// Shortcuts for common cases.  (Implemented as inline functions below)
	bool SetGlobalConfigValueInt32( EGameNetworkingConfigValue eValue, int32 val );
	bool SetGlobalConfigValueFloat( EGameNetworkingConfigValue eValue, float val );
	bool SetGlobalConfigValueString( EGameNetworkingConfigValue eValue, const char *val );
	bool SetGlobalConfigValuePtr( EGameNetworkingConfigValue eValue, void *val );
	bool SetConnectionConfigValueInt32( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, int32 val );
	bool SetConnectionConfigValueFloat( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, float val );
	bool SetConnectionConfigValueString( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, const char *val );

	//
	// Set global callbacks.  If you do not want to use Steam's callback dispatch mechanism and you
	// want to use the same callback on all (or most) listen sockets and connections, then
	// simply install these callbacks first thing, and you are good to go.
	// See IGameNetworkingSockets::RunCallbacks
	//
	bool SetGlobalCallback_GameNetConnectionStatusChanged( FnGameNetConnectionStatusChanged fnCallback );
	bool SetGlobalCallback_GameNetAuthenticationStatusChanged( FnGameNetAuthenticationStatusChanged fnCallback );
	bool SetGlobalCallback_SteamRelayNetworkStatusChanged( FnSteamRelayNetworkStatusChanged fnCallback );
	bool SetGlobalCallback_MessagesSessionRequest( FnGameNetworkingMessagesSessionRequest fnCallback );
	bool SetGlobalCallback_MessagesSessionFailed( FnGameNetworkingMessagesSessionFailed fnCallback );

	/// Set a configuration value.
	/// - eValue: which value is being set
	/// - eScope: Onto what type of object are you applying the setting?
	/// - scopeArg: Which object you want to change?  (Ignored for global scope).  E.g. connection handle, listen socket handle, interface pointer, etc.
	/// - eDataType: What type of data is in the buffer at pValue?  This must match the type of the variable exactly!
	/// - pArg: Value to set it to.  You can pass NULL to remove a non-global setting at this scope,
	///   causing the value for that object to use global defaults.  Or at global scope, passing NULL
	///   will reset any custom value and restore it to the system default.
	///   NOTE: When setting pointers (e.g. callback functions), do not pass the function pointer directly.
	///   Your argument should be a pointer to a function pointer.
	virtual bool SetConfigValue( EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj,
		EGameNetworkingConfigDataType eDataType, const void *pArg ) = 0;

    virtual AppId_t GetAppID() = 0;
    virtual void SetAppID( AppId_t nAppID ) = 0;

	/// Set a configuration value, using a struct to pass the value.
	/// (This is just a convenience shortcut; see below for the implementation and
	/// a little insight into how GameNetworkingConfigValue_t is used when
	/// setting config options during listen socket and connection creation.)
	bool SetConfigValueStruct( const GameNetworkingConfigValue_t &opt, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj );

	/// Get a configuration value.
	/// - eValue: which value to fetch
	/// - eScopeType: query setting on what type of object
	/// - eScopeArg: the object to query the setting for
	/// - pOutDataType: If non-NULL, the data type of the value is returned.
	/// - pResult: Where to put the result.  Pass NULL to query the required buffer size.  (k_EGameNetworkingGetConfigValue_BufferTooSmall will be returned.)
	/// - cbResult: IN: the size of your buffer.  OUT: the number of bytes filled in or required.
	virtual EGameNetworkingGetConfigValueResult GetConfigValue( EGameNetworkingConfigValue eValue, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj,
		EGameNetworkingConfigDataType *pOutDataType, void *pResult, size_t *cbResult ) = 0;

	/// Returns info about a configuration value.  Returns false if the value does not exist.
	/// pOutNextValue can be used to iterate through all of the known configuration values.
	/// (Use GetFirstConfigValue() to begin the iteration, will be k_EGameNetworkingConfig_Invalid on the last value)
	/// Any of the output parameters can be NULL if you do not need that information.
	///
	/// See k_EGameNetworkingConfig_EnumerateDevVars for some more info about "dev" variables,
	/// which are usually excluded from the set of variables enumerated using this function.
	virtual bool GetConfigValueInfo( EGameNetworkingConfigValue eValue, const char **pOutName, EGameNetworkingConfigDataType *pOutDataType, EGameNetworkingConfigScope *pOutScope, EGameNetworkingConfigValue *pOutNextValue ) = 0;

	/// Return the lowest numbered configuration value available in the current environment.
	virtual EGameNetworkingConfigValue GetFirstConfigValue() = 0;

	// String conversions.  You'll usually access these using the respective
	// inline methods.
	virtual void GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr &addr, char *buf, size_t cbBuf, bool bWithPort ) = 0;
	virtual bool GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr *pAddr, const char *pszStr ) = 0;
	virtual void GameNetworkingIdentity_ToString( const GameNetworkingIdentity &identity, char *buf, size_t cbBuf ) = 0;
	virtual bool GameNetworkingIdentity_ParseString( GameNetworkingIdentity *pIdentity, const char *pszStr ) = 0;

protected:
	~IGameNetworkingUtils(); // Silence some warnings
};
#define STEAMNETWORKINGUTILS_INTERFACE_VERSION "GameNetworkingUtils003"

// Global accessors
// Using standalone lib
#ifdef STEAMNETWORKINGSOCKETS_STANDALONELIB

	// Standalone lib
	static_assert( STEAMNETWORKINGUTILS_INTERFACE_VERSION[21] == '3', "Version mismatch" );
	STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingUtils *GameNetworkingUtils_LibV3();
	inline IGameNetworkingUtils *GameNetworkingUtils_Lib() { return GameNetworkingUtils_LibV3(); }

	#ifndef STEAMNETWORKINGSOCKETS_STEAMAPI
		inline IGameNetworkingUtils *GameNetworkingUtils() { return GameNetworkingUtils_LibV3(); }
	#endif
#endif

// Using Steamworks SDK
#ifdef STEAMNETWORKINGSOCKETS_STEAMAPI
	STEAM_DEFINE_INTERFACE_ACCESSOR( IGameNetworkingUtils *, GameNetworkingUtils_SteamAPI,
		/* Prefer user version of the interface.  But if it isn't found, then use
		gameserver one.  Yes, this is a completely terrible hack */
		SteamInternal_FindOrCreateUserInterface( 0, STEAMNETWORKINGUTILS_INTERFACE_VERSION ) ?
		SteamInternal_FindOrCreateUserInterface( 0, STEAMNETWORKINGUTILS_INTERFACE_VERSION ) :
		SteamInternal_FindOrCreateGameServerInterface( 0, STEAMNETWORKINGUTILS_INTERFACE_VERSION ),
		"global",
		STEAMNETWORKINGUTILS_INTERFACE_VERSION
	)

	#ifndef STEAMNETWORKINGSOCKETS_STANDALONELIB
		inline IGameNetworkingUtils *GameNetworkingUtils() { return GameNetworkingUtils_SteamAPI(); }
	#endif
#endif

/// A struct used to describe our readiness to use the relay network.
/// To do this we first need to fetch the network configuration,
/// which describes what POPs are available.
struct SteamRelayNetworkStatus_t
{ 
	enum { k_iCallback = k_iGameNetworkingUtilsCallbacks + 1 };

	/// Summary status.  When this is "current", initialization has
	/// completed.  Anything else means you are not ready yet, or
	/// there is a significant problem.
	EGameNetworkingAvailability m_eAvail;

	/// Nonzero if latency measurement is in progress (or pending,
	/// awaiting a prerequisite).
	int m_bPingMeasurementInProgress;

	/// Status obtaining the network config.  This is a prerequisite
	/// for relay network access.
	///
	/// Failure to obtain the network config almost always indicates
	/// a problem with the local internet connection.
	EGameNetworkingAvailability m_eAvailNetworkConfig;

	/// Current ability to communicate with ANY relay.  Note that
	/// the complete failure to communicate with any relays almost
	/// always indicates a problem with the local Internet connection.
	/// (However, just because you can reach a single relay doesn't
	/// mean that the local connection is in perfect health.)
	EGameNetworkingAvailability m_eAvailAnyRelay;

	/// Non-localized English language status.  For diagnostic/debugging
	/// purposes only.
	char m_debugMsg[ 256 ];
};

/// Utility class for printing a GameNetworkingIdentity.
/// E.g. printf( "Identity is '%s'\n", GameNetworkingIdentityRender( identity ).c_str() );
struct GameNetworkingIdentityRender
{
	GameNetworkingIdentityRender( const GameNetworkingIdentity &x ) { x.ToString( buf, sizeof(buf) ); }
	inline const char *c_str() const { return buf; }
private:
	char buf[ GameNetworkingIdentity::k_cchMaxString ];
};

/// Utility class for printing a GameNetworkingIPAddrRender.
struct GameNetworkingIPAddrRender
{
	GameNetworkingIPAddrRender( const GameNetworkingIPAddr &x, bool bWithPort = true ) { x.ToString( buf, sizeof(buf), bWithPort ); }
	inline const char *c_str() const { return buf; }
private:
	char buf[ GameNetworkingIPAddr::k_cchMaxString ];
};

///////////////////////////////////////////////////////////////////////////////
//
// Internal stuff

inline void IGameNetworkingUtils::InitRelayNetworkAccess() { CheckPingDataUpToDate( 1e10f ); }
inline bool IGameNetworkingUtils::SetGlobalConfigValueInt32( EGameNetworkingConfigValue eValue, int32 val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Global, 0, k_EGameNetworkingConfig_Int32, &val ); }
inline bool IGameNetworkingUtils::SetGlobalConfigValueFloat( EGameNetworkingConfigValue eValue, float val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Global, 0, k_EGameNetworkingConfig_Float, &val ); }
inline bool IGameNetworkingUtils::SetGlobalConfigValueString( EGameNetworkingConfigValue eValue, const char *val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Global, 0, k_EGameNetworkingConfig_String, val ); }
inline bool IGameNetworkingUtils::SetGlobalConfigValuePtr( EGameNetworkingConfigValue eValue, void *val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Global, 0, k_EGameNetworkingConfig_Ptr, &val ); } // Note: passing pointer to pointer.
inline bool IGameNetworkingUtils::SetConnectionConfigValueInt32( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, int32 val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Connection, hConn, k_EGameNetworkingConfig_Int32, &val ); }
inline bool IGameNetworkingUtils::SetConnectionConfigValueFloat( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, float val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Connection, hConn, k_EGameNetworkingConfig_Float, &val ); }
inline bool IGameNetworkingUtils::SetConnectionConfigValueString( HGameNetConnection hConn, EGameNetworkingConfigValue eValue, const char *val ) { return SetConfigValue( eValue, k_EGameNetworkingConfig_Connection, hConn, k_EGameNetworkingConfig_String, val ); }
inline bool IGameNetworkingUtils::SetGlobalCallback_GameNetConnectionStatusChanged( FnGameNetConnectionStatusChanged fnCallback ) { return SetGlobalConfigValuePtr( k_EGameNetworkingConfig_Callback_ConnectionStatusChanged, (void*)fnCallback ); }
inline bool IGameNetworkingUtils::SetGlobalCallback_GameNetAuthenticationStatusChanged( FnGameNetAuthenticationStatusChanged fnCallback ) { return SetGlobalConfigValuePtr( k_EGameNetworkingConfig_Callback_AuthStatusChanged, (void*)fnCallback ); }
inline bool IGameNetworkingUtils::SetGlobalCallback_SteamRelayNetworkStatusChanged( FnSteamRelayNetworkStatusChanged fnCallback ) { return SetGlobalConfigValuePtr( k_EGameNetworkingConfig_Callback_RelayNetworkStatusChanged, (void*)fnCallback ); }
inline bool IGameNetworkingUtils::SetGlobalCallback_MessagesSessionRequest( FnGameNetworkingMessagesSessionRequest fnCallback ) { return SetGlobalConfigValuePtr( k_EGameNetworkingConfig_Callback_MessagesSessionRequest, (void*)fnCallback ); }
inline bool IGameNetworkingUtils::SetGlobalCallback_MessagesSessionFailed( FnGameNetworkingMessagesSessionFailed fnCallback ) { return SetGlobalConfigValuePtr( k_EGameNetworkingConfig_Callback_MessagesSessionFailed, (void*)fnCallback ); }

inline bool IGameNetworkingUtils::SetConfigValueStruct( const GameNetworkingConfigValue_t &opt, EGameNetworkingConfigScope eScopeType, intptr_t scopeObj )
{
	// Locate the argument.  Strings are a special case, since the
	// "value" (the whole string buffer) doesn't fit in the struct
	// NOTE: for pointer values, we pass a pointer to the pointer,
	// we do not pass the pointer directly.
	const void *pVal = ( opt.m_eDataType == k_EGameNetworkingConfig_String ) ? (const void *)opt.m_val.m_string : (const void *)&opt.m_val;
	return SetConfigValue( opt.m_eValue, eScopeType, scopeObj, opt.m_eDataType, pVal );
}

// How to get helper functions.
#if defined( STEAMNETWORKINGSOCKETS_STATIC_LINK ) || defined( STEAMNETWORKINGSOCKETS_STANDALONELIB )

	// Call direct to static functions
	STEAMNETWORKINGSOCKETS_INTERFACE void GameNetworkingIPAddr_ToString( const GameNetworkingIPAddr *pAddr, char *buf, size_t cbBuf, bool bWithPort );
	STEAMNETWORKINGSOCKETS_INTERFACE bool GameNetworkingIPAddr_ParseString( GameNetworkingIPAddr *pAddr, const char *pszStr );
	STEAMNETWORKINGSOCKETS_INTERFACE void GameNetworkingIdentity_ToString( const GameNetworkingIdentity *pIdentity, char *buf, size_t cbBuf );
	STEAMNETWORKINGSOCKETS_INTERFACE bool GameNetworkingIdentity_ParseString( GameNetworkingIdentity *pIdentity, size_t sizeofIdentity, const char *pszStr );
	inline void GameNetworkingIPAddr::ToString( char *buf, size_t cbBuf, bool bWithPort ) const { GameNetworkingIPAddr_ToString( this, buf, cbBuf, bWithPort ); }
	inline bool GameNetworkingIPAddr::ParseString( const char *pszStr ) { return GameNetworkingIPAddr_ParseString( this, pszStr ); }
	inline void GameNetworkingIdentity::ToString( char *buf, size_t cbBuf ) const { GameNetworkingIdentity_ToString( this, buf, cbBuf ); }
	inline bool GameNetworkingIdentity::ParseString( const char *pszStr ) { return GameNetworkingIdentity_ParseString( this, sizeof(*this), pszStr ); }

#elif defined( STEAMNETWORKINGSOCKETS_STEAMAPI )
	// Using gameworks SDK - go through GameNetworkingUtils()
	inline void GameNetworkingIPAddr::ToString( char *buf, size_t cbBuf, bool bWithPort ) const { GameNetworkingUtils()->GameNetworkingIPAddr_ToString( *this, buf, cbBuf, bWithPort ); }
	inline bool GameNetworkingIPAddr::ParseString( const char *pszStr ) { return GameNetworkingUtils()->GameNetworkingIPAddr_ParseString( this, pszStr ); }
	inline void GameNetworkingIdentity::ToString( char *buf, size_t cbBuf ) const { GameNetworkingUtils()->GameNetworkingIdentity_ToString( *this, buf, cbBuf ); }
	inline bool GameNetworkingIdentity::ParseString( const char *pszStr ) { return GameNetworkingUtils()->GameNetworkingIdentity_ParseString( this, pszStr ); }
#else
	#error "Invalid config"
#endif

#endif // ISTEAMNETWORKINGUTILS

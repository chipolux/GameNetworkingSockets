//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef ISTEAMNETWORKINGMESSAGES
#define ISTEAMNETWORKINGMESSAGES
#pragma once

#include "gamenetworkingtypes.h"
#include "game_api_common.h"

//-----------------------------------------------------------------------------
/// The non-connection-oriented interface to send and receive messages
/// (whether they be "clients" or "servers").
///
/// IGameNetworkingSockets is connection-oriented (like TCP), meaning you
/// need to listen and connect, and then you send messages using a connection
/// handle.  IGameNetworkingMessages is more like UDP, in that you can just send
/// messages to arbitrary peers at any time.  The underlying connections are
/// established implicitly.
///
/// Under the hood IGameNetworkingMessages works on top of the IGameNetworkingSockets
/// code, so you get the same routing and messaging efficiency.  The difference is
/// mainly in your responsibility to explicitly establish a connection and
/// the type of feedback you get about the state of the connection.  Both
/// interfaces can do "P2P" communications, and both support both unreliable
/// and reliable messages, fragmentation and reassembly.
///
/// The primary purpose of this interface is to be "like UDP", so that UDP-based code
/// can be ported easily to take advantage of relayed connections.  If you find
/// yourself needing more low level information or control, or to be able to better
/// handle failure, then you probably need to use IGameNetworkingSockets directly.
/// Also, note that if your main goal is to obtain a connection between two peers
/// without concerning yourself with assigning roles of "client" and "server",
/// you may find the symmetric connection mode of IGameNetworkingSockets useful.
/// (See k_EGameNetworkingConfig_SymmetricConnect.)
///
class IGameNetworkingMessages
{
public:
	/// Sends a message to the specified host.  If we don't already have a session with that user,
	/// a session is implicitly created.  There might be some handshaking that needs to happen
	/// before we can actually begin sending message data.  If this handshaking fails and we can't
	/// get through, an error will be posted via the callback GameNetworkingMessagesSessionFailed_t.
	/// There is no notification when the operation succeeds.  (You should have the peer send a reply
	/// for this purpose.)
	///
	/// Sending a message to a host will also implicitly accept any incoming connection from that host.
	///
	/// nSendFlags is a bitmask of k_nGameNetworkingSend_xxx options
	///
	/// nRemoteChannel is a routing number you can use to help route message to different systems.
	/// You'll have to call ReceiveMessagesOnChannel() with the same channel number in order to retrieve
	/// the data on the other end.
	///
	/// Using different channels to talk to the same user will still use the same underlying
	/// connection, saving on resources.  If you don't need this feature, use 0.
	/// Otherwise, small integers are the most efficient.
	///
	/// It is guaranteed that reliable messages to the same host on the same channel
	/// will be be received by the remote host (if they are received at all) exactly once,
	/// and in the same order that they were sent.
	///
	/// NO other order guarantees exist!  In particular, unreliable messages may be dropped,
	/// received out of order with respect to each other and with respect to reliable data,
	/// or may be received multiple times.  Messages on different channels are *not* guaranteed
	/// to be received in the order they were sent.
	///
	/// A note for those familiar with TCP/IP ports, or converting an existing codebase that
	/// opened multiple sockets:  You might notice that there is only one channel, and with
	/// TCP/IP each endpoint has a port number.  You can think of the channel number as the
	/// *destination* port.  If you need each message to also include a "source port" (so the
	/// recipient can route the reply), then just put that in your message.  That is essentially
	/// how UDP works!
	///
	/// Returns:
	/// - k_EREsultOK on success.
	/// - k_EResultNoConnection will be returned if the session has failed or was closed by the peer,
	///   and k_nGameNetworkingSend_AutoRestartBrokenSession is not used.  (You can use
	///   GetSessionConnectionInfo to get the details.)  In order to acknowledge the broken session
	///   and start a new one, you must call CloseSessionWithUser
	/// - See IGameNetworkingSockets::SendMessageToConnection for more possible return values
	virtual EResult SendMessageToUser( const GameNetworkingIdentity &identityRemote, const void *pubData, uint32 cubData, int nSendFlags, int nRemoteChannel ) = 0;

	/// Reads the next message that has been sent from another user via SendMessageToUser() on the given channel.
	/// Returns number of messages returned into your list.  (0 if no message are available on that channel.)
	///
	/// When you're done with the message object(s), make sure and call GameNetworkingMessage_t::Release()!
	virtual int ReceiveMessagesOnChannel( int nLocalChannel, GameNetworkingMessage_t **ppOutMessages, int nMaxMessages ) = 0;

	/// Call this in response to a GameNetworkingMessagesSessionRequest_t callback.
	/// GameNetworkingMessagesSessionRequest_t are posted when a user tries to send you a message,
	/// and you haven't tried to talk to them first.  If you don't want to talk to them, just ignore
	/// the request.  If the user continues to send you messages, GameNetworkingMessagesSessionRequest_t
	/// callbacks will continue to be posted periodically.
	///
	/// Returns false if there is no session with the user pending or otherwise.  If there is an
	/// existing active session, this function will return true, even if it is not pending.
	///
	/// Calling SendMessageToUser() will implicitly accepts any pending session request to that user.
	virtual bool AcceptSessionWithUser( const GameNetworkingIdentity &identityRemote ) = 0;

	/// Call this when you're done talking to a user to immediately free up resources under-the-hood.
	/// If the remote user tries to send data to you again, another GameNetworkingMessagesSessionRequest_t
	/// callback will be posted.
	///
	/// Note that sessions that go unused for a few minutes are automatically timed out.
	virtual bool CloseSessionWithUser( const GameNetworkingIdentity &identityRemote ) = 0;

	/// Call this  when you're done talking to a user on a specific channel.  Once all
	/// open channels to a user have been closed, the open session to the user will be
	/// closed, and any new data from this user will trigger a
	/// SteamGameNetworkingMessagesSessionRequest_t callback
	virtual bool CloseChannelWithUser( const GameNetworkingIdentity &identityRemote, int nLocalChannel ) = 0;

	/// Returns information about the latest state of a connection, if any, with the given peer.
	/// Primarily intended for debugging purposes, but can also be used to get more detailed
	/// failure information.  (See SendMessageToUser and k_nGameNetworkingSend_AutoRestartBrokenSession.)
	///
	/// Returns the value of GameNetConnectionInfo_t::m_eState, or k_EGameNetworkingConnectionState_None
	/// if no connection exists with specified peer.  You may pass nullptr for either parameter if
	/// you do not need the corresponding details.  Note that sessions time out after a while,
	/// so if a connection fails, or SendMessageToUser returns k_EResultNoConnection, you cannot wait
	/// indefinitely to obtain the reason for failure.
	virtual EGameNetworkingConnectionState GetSessionConnectionInfo( const GameNetworkingIdentity &identityRemote, GameNetConnectionInfo_t *pConnectionInfo, GameNetworkingQuickConnectionStatus *pQuickStatus ) = 0;
};
#define STEAMNETWORKINGMESSAGES_INTERFACE_VERSION "GameNetworkingMessages002"

//
// Callbacks
//

#pragma pack( push, 1 )

/// Posted when a remote host is sending us a message, and we do not already have a session with them
struct GameNetworkingMessagesSessionRequest_t
{ 
	enum { k_iCallback = k_iGameNetworkingMessagesCallbacks + 1 };
	GameNetworkingIdentity m_identityRemote;			// user who wants to talk to us
};

/// Posted when we fail to establish a connection, or we detect that communications
/// have been disrupted it an unusual way.  There is no notification when a peer proactively
/// closes the session.  ("Closed by peer" is not a concept of UDP-style communications, and
/// GameNetworkingMessages is primarily intended to make porting UDP code easy.)
///
/// Remember: callbacks are asynchronous.   See notes on SendMessageToUser,
/// and k_nGameNetworkingSend_AutoRestartBrokenSession in particular.
///
/// Also, if a session times out due to inactivity, no callbacks will be posted.  The only
/// way to detect that this is happening is that querying the session state may return
/// none, connecting, and findingroute again.
struct GameNetworkingMessagesSessionFailed_t
{ 
	enum { k_iCallback = k_iGameNetworkingMessagesCallbacks + 2 };

	/// Detailed info about the session that failed.
	/// GameNetConnectionInfo_t::m_identityRemote indicates who this session
	/// was with.
	GameNetConnectionInfo_t m_info;
};

#pragma pack(pop)

// Global accessors
// Using standalone lib
#ifdef STEAMNETWORKINGSOCKETS_STANDALONELIB

	// Standalone lib.
	static_assert( STEAMNETWORKINGMESSAGES_INTERFACE_VERSION[24] == '2', "Version mismatch" );
	STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingMessages *GameNetworkingMessages_LibV2();
	inline IGameNetworkingMessages *GameNetworkingMessages_Lib() { return GameNetworkingMessages_LibV2(); }

	// If running in context of game, we also define a gameserver instance.
	#ifdef STEAMNETWORKINGSOCKETS_STEAM
		STEAMNETWORKINGSOCKETS_INTERFACE IGameNetworkingMessages *SteamGameServerNetworkingMessages_LibV2();
		inline IGameNetworkingMessages *SteamGameServerNetworkingMessages_Lib() { return SteamGameServerNetworkingMessages_LibV2(); }
	#endif

	#ifndef STEAMNETWORKINGSOCKETS_STEAMAPI
		inline IGameNetworkingMessages *GameNetworkingMessages() { return GameNetworkingMessages_LibV2(); }
		#ifdef STEAMNETWORKINGSOCKETS_STEAM
			inline IGameNetworkingMessages *SteamGameServerNetworkingMessages() { return SteamGameServerNetworkingMessages_LibV2(); }
		#endif
	#endif
#endif

// Using Steamworks SDK
#ifdef STEAMNETWORKINGSOCKETS_STEAMAPI

	// Steamworks SDK
	STEAM_DEFINE_USER_INTERFACE_ACCESSOR( IGameNetworkingMessages *, GameNetworkingMessages_SteamAPI, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION );
	STEAM_DEFINE_GAMESERVER_INTERFACE_ACCESSOR( IGameNetworkingMessages *, SteamGameServerNetworkingMessages_SteamAPI, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION );

	#ifndef STEAMNETWORKINGSOCKETS_STANDALONELIB
		inline IGameNetworkingMessages *GameNetworkingMessages() { return GameNetworkingMessages_SteamAPI(); }
		inline IGameNetworkingMessages *SteamGameServerNetworkingMessages() { return SteamGameServerNetworkingMessages_SteamAPI(); }
	#endif
#endif

#endif // ISTEAMNETWORKINGMESSAGES

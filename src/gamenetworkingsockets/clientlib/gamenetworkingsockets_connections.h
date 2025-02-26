//====== Copyright Valve Corporation, All rights reserved. ====================

#ifndef STEAMNETWORKINGSOCKETS_CONNECTIONS_H
#define STEAMNETWORKINGSOCKETS_CONNECTIONS_H
#pragma once

#include "../gamenetworkingsockets_internal.h"
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include "../gamedatagram_internal.h"
#include <gns/gamedatagram_tickets.h>
#endif
#include "../gamenetworking_statsutils.h"
#include <tier1/utlhashmap.h>
#include "gamenetworkingsockets_lowlevel.h"
#include "../gamenetworkingsockets_thinker.h"
#include "keypair.h"
#include "crypto.h"
#include "crypto_25519.h"
#include <tier0/memdbgoff.h>
#include <gamenetworkingsockets_messages.pb.h>
#include <tier0/memdbgon.h>

#include "gamenetworkingsockets_snp.h"

struct GameNetConnectionStatusChangedCallback_t;
class IGameNetworkingSocketsSerialized;

namespace GameNetworkingSocketsLib {

const GameNetworkingMicroseconds k_usecConnectRetryInterval = k_nMillion/2;
const GameNetworkingMicroseconds k_usecFinWaitTimeout = 5*k_nMillion;

typedef char ConnectionEndDebugMsg[ k_cchGameNetworkingMaxConnectionCloseReason ];
typedef char ConnectionTypeDescription_t[64];

class CGameNetworkingSockets;
class CGameNetworkingMessages;
class CGameNetworkConnectionBase;
class CGameNetworkConnectionP2P;
class CSharedSocket;
class CConnectionTransport;
struct SNPAckSerializerHelper;
struct CertAuthScope;

enum EUnsignedCert
{
	k_EUnsignedCert_Disallow,
	k_EUnsignedCert_AllowWarn,
	k_EUnsignedCert_Allow,
};

// Fixed size byte array that automatically wipes itself upon destruction.
// Used for storage of secret keys, etc.
template <int N>
class AutoWipeFixedSizeBuffer
{
public:
	enum { k_nSize = N };
	uint8 m_buf[ N ];

	// You can wipe before destruction if you want
	inline void Wipe() { SecureZeroMemory( m_buf, N ); }

	// Wipe on destruction
	inline ~AutoWipeFixedSizeBuffer() { Wipe(); }
};

/// In various places, we need a key in a map of remote connections.
struct RemoteConnectionKey_t
{
	GameNetworkingIdentity m_identity;
	uint32 m_unConnectionID;

	// NOTE: If we assume that peers are well behaved, then we
	// could just use the connection ID, which is a random number.
	// but let's not assume that.  In fact, if we really need to
	// protect against malicious clients we might have to include
	// some random private data so that they don't know how our hash
	// function works.  We'll assume for now that this isn't a problem
	struct Hash { uint32 operator()( const RemoteConnectionKey_t &x ) const { return GameNetworkingIdentityHash{}( x.m_identity ) ^ x.m_unConnectionID; } };
	inline bool operator ==( const RemoteConnectionKey_t &x ) const
	{
		return m_unConnectionID == x.m_unConnectionID && m_identity == x.m_identity;
	}
};

/// Base class for connection-type-specific context structure 
struct SendPacketContext_t
{
	inline SendPacketContext_t( GameNetworkingMicroseconds usecNow, const char *pszReason ) : m_usecNow( usecNow ), m_pszReason( pszReason ) {}
	const GameNetworkingMicroseconds m_usecNow;
	int m_cbMaxEncryptedPayload;
	const char *m_pszReason; // Why are we sending this packet?
};

/// Context used when receiving a data packet
struct RecvPacketContext_t
{

//
// Must be filled in by transport
//

	/// Current time
	GameNetworkingMicroseconds m_usecNow;

	/// What transport is receiving this packet?
	CConnectionTransport *m_pTransport;

	/// Jitter measurement, if present
	//int m_usecTimeSinceLast;

//
// Output of DecryptDataChunk
//

	/// Expanded packet number
	int64 m_nPktNum;

	/// Pointer to decrypted data.  Will either point to to the caller's original packet,
	/// if the packet was not encrypted, or m_decrypted, if it was encrypted and we
	/// decrypted it
	const void *m_pPlainText;

	/// Size of plaintext
	int m_cbPlainText;

	// Temporary buffer to hold decrypted data, if we were actually encrypted
	uint8 m_decrypted[ k_cbGameNetworkingSocketsMaxPlaintextPayloadRecv ];
};

template<typename TStatsMsg>
struct SendPacketContext : SendPacketContext_t
{
	inline SendPacketContext( GameNetworkingMicroseconds usecNow, const char *pszReason ) : SendPacketContext_t( usecNow, pszReason ) {}

	uint32 m_nFlags; // Message flags that we need to set.
	TStatsMsg msg; // Type-specific stats message
	int m_cbMsgSize; // Size of message
	int m_cbTotalSize; // Size needed in the header, including the serialized size field

	void SlamFlagsAndCalcSize()
	{
		SetStatsMsgFlagsIfNotImplied( msg, m_nFlags );
		m_cbTotalSize = m_cbMsgSize = ProtoMsgByteSize( msg );
		if ( m_cbMsgSize > 0 )
			m_cbTotalSize += VarIntSerializedSize( (uint32)m_cbMsgSize );
	}

	bool Serialize( byte *&p )
	{
		if ( m_cbTotalSize <= 0 )
			return false;

		// Serialize the stats size, var-int encoded
		byte *pOut = SerializeVarInt( p, uint32( m_cbMsgSize ) );

		// Serialize the actual message
		pOut = msg.SerializeWithCachedSizesToArray( pOut );

		// Make sure we wrote the number of bytes we expected
		if ( pOut != p + m_cbTotalSize )
		{
			// ABORT!
			AssertMsg( false, "Size mismatch after serializing inline stats blob" );
			return false;
		}

		// Advance pointer
		p = pOut;
		return true;
	}

	void CalcMaxEncryptedPayloadSize( size_t cbHdrReserve, CGameNetworkConnectionBase *pConnection );
};

/// Replace internal states that are not visible outside of the API with
/// the corresponding state that we show the the application.
inline EGameNetworkingConnectionState CollapseConnectionStateToAPIState( EGameNetworkingConnectionState eState )
{
	// All the hidden internal states are assigned negative values
	if ( eState < 0 )
		return k_EGameNetworkingConnectionState_None;
	return eState;
}

/// We use one global lock to protect all queues of
/// received messages.  (On connections and poll groups!)
extern ShortDurationLock g_lockAllRecvMessageQueues;

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkPollGroup
//
/////////////////////////////////////////////////////////////////////////////

class CGameNetworkPollGroup;
struct PollGroupLock : Lock<RecursiveTimedMutexImpl> {
	PollGroupLock() : Lock<RecursiveTimedMutexImpl>( "pollgroup", LockDebugInfo::k_nFlag_PollGroup ) {}
};
using PollGroupScopeLock = ScopeLock<PollGroupLock>;

class CGameNetworkPollGroup
{
public:
	CGameNetworkPollGroup( CGameNetworkingSockets *pInterface );
	~CGameNetworkPollGroup();

	PollGroupLock m_lock;

	/// What interface is responsible for this listen socket?
	CGameNetworkingSockets *const m_pGameNetworkingSocketsInterface;

	/// Linked list of messages received through any connection on this listen socket
	GameNetworkingMessageQueue m_queueRecvMessages;

	/// Index into the global list
	HGameNetPollGroup m_hPollGroupSelf;

	/// List of connections that are in this poll group
	CUtlVector<CGameNetworkConnectionBase *> m_vecConnections;

	void AssignHandleAndAddToGlobalTable();
};

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkListenSocketBase
//
/////////////////////////////////////////////////////////////////////////////

/// Abstract base class for a listen socket that can accept connections.
class CGameNetworkListenSocketBase
{
public:

	/// Destroy the listen socket, and all of its accepted connections
	virtual void Destroy();

	/// Called when we receive a connection attempt, to setup the linkage.
	bool BAddChildConnection( CGameNetworkConnectionBase *pConn, GameNetworkingErrMsg &errMsg );

	/// This gets called on an accepted connection before it gets destroyed
	virtual void AboutToDestroyChildConnection( CGameNetworkConnectionBase *pConn );

	virtual bool APIGetAddress( GameNetworkingIPAddr *pAddress );

	/// Map of child connections
	CUtlHashMap<RemoteConnectionKey_t, CGameNetworkConnectionBase *, std::equal_to<RemoteConnectionKey_t>, RemoteConnectionKey_t::Hash > m_mapChildConnections;

	/// Index into the global list
	HSteamListenSocket m_hListenSocketSelf;

	/// What interface is responsible for this listen socket?
	CGameNetworkingSockets *const m_pGameNetworkingSocketsInterface;

	/// Configuration options that will apply to all connections accepted through this listen socket
	ConnectionConfig m_connectionConfig;

	/// Symmetric mode
	inline bool BSymmetricMode() const { return m_connectionConfig.m_SymmetricConnect.Get() != 0; }
	virtual bool BSupportsSymmetricMode();

	/// For legacy interface.
	#ifdef STEAMNETWORKINGSOCKETS_STEAMCLIENT
	CGameNetworkPollGroup m_legacyPollGroup;
	#endif

protected:
	CGameNetworkListenSocketBase( CGameNetworkingSockets *pGameNetworkingSocketsInterface );
	virtual ~CGameNetworkListenSocketBase(); // hidden destructor, don't call directly.  Use Destroy()

	bool BInitListenSocketCommon( int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg );
};

/////////////////////////////////////////////////////////////////////////////
//
// CGameNetworkConnectionBase
//
/////////////////////////////////////////////////////////////////////////////

struct ConnectionLock : Lock<RecursiveTimedMutexImpl> {
	ConnectionLock() : Lock<RecursiveTimedMutexImpl>( "connection", LockDebugInfo::k_nFlag_Connection ) {}
};
struct ConnectionScopeLock : ScopeLock<ConnectionLock>
{
	ConnectionScopeLock() = default;
	ConnectionScopeLock( ConnectionLock &lock, const char *pszTag = nullptr ) : ScopeLock<ConnectionLock>( lock, pszTag ) {}
	ConnectionScopeLock( CGameNetworkConnectionBase &conn, const char *pszTag = nullptr );
	void Lock( ConnectionLock &lock, const char *pszTag = nullptr ) { ScopeLock<ConnectionLock>::Lock( lock, pszTag ); }
	void Lock( CGameNetworkConnectionBase &conn, const char *pszTag = nullptr );
};

/// Abstract interface for a connection to a remote host over any underlying
/// transport.  Most of the common functionality for implementing reliable
/// connections on top of unreliable datagrams, connection quality measurement,
/// etc is implemented here. 
class CGameNetworkConnectionBase : public ILockableThinker< ConnectionLock >
{
public:

//
// API entry points
//

	/// Called when we close the connection locally
	void APICloseConnection( int nReason, const char *pszDebug, bool bEnableLinger );

	/// Send a message
	EResult APISendMessageToConnection( const void *pData, uint32 cbData, int nSendFlags, int64 *pOutMessageNumber );

	/// Send a message.  Returns the assigned message number, or a negative EResult value
	int64 APISendMessageToConnection( CGameNetworkingMessage *pMsg, GameNetworkingMicroseconds usecNow, bool *pbThinkImmediately = nullptr );

	/// Flush any messages queued for Nagle
	EResult APIFlushMessageOnConnection();

	/// Receive the next message(s)
	int APIReceiveMessages( GameNetworkingMessage_t **ppOutMessages, int nMaxMessages );

	/// Accept a connection.  This will involve sending a message
	/// to the client, and calling ConnectionState_Connected on the connection
	/// to transition it to the connected state.
	EResult APIAcceptConnection();
	virtual EResult AcceptConnection( GameNetworkingMicroseconds usecNow );

	/// Fill in quick connection stats
	void APIGetQuickConnectionStatus( GameNetworkingQuickConnectionStatus &stats );

	/// Fill in detailed connection stats
	virtual void APIGetDetailedConnectionStatus( GameNetworkingDetailedConnectionStatus &stats, GameNetworkingMicroseconds usecNow );

	/// Hook to allow connections to customize message sending.
	/// (E.g. loopback.)
	virtual int64 _APISendMessageToConnection( CGameNetworkingMessage *pMsg, GameNetworkingMicroseconds usecNow, bool *pbThinkImmediately );

//
// Accessor
//

	// Get/set user data
	inline int64 GetUserData() const
	{
		// User data is locked when we create a connection!
		Assert( m_connectionConfig.m_ConnectionUserData.IsSet() );
		return m_connectionConfig.m_ConnectionUserData.m_data;
	}
	void SetUserData( int64 nUserData );

	// Get/set name
	inline const char *GetAppName() const { return m_szAppName; }
	void SetAppName( const char *pszName );

	// Debug description
	inline const char *GetDescription() const { return m_szDescription; }

	/// When something changes that goes into the description, call this to rebuild the description
	void SetDescription();

	/// High level state of the connection
	EGameNetworkingConnectionState GetState() const { return m_eConnectionState; }
	EGameNetworkingConnectionState GetWireState() const { return m_eConnectionWireState; }

	/// Check if the connection is 'connected' from the perspective of the wire protocol.
	/// (The wire protocol doesn't care about local states such as linger)
	bool BStateIsConnectedForWirePurposes() const { return m_eConnectionWireState == k_EGameNetworkingConnectionState_Connected; }

	/// Return true if the connection is still "active" in some way.
	bool BStateIsActive() const
	{
		return
			m_eConnectionWireState == k_EGameNetworkingConnectionState_Connecting
			|| m_eConnectionWireState == k_EGameNetworkingConnectionState_FindingRoute
			|| m_eConnectionWireState == k_EGameNetworkingConnectionState_Connected;
	}

	/// Reason connection ended
	EGameNetConnectionEnd GetConnectionEndReason() const { return m_eEndReason; }
	const char *GetConnectionEndDebugString() const { return m_szEndDebug; }

	/// When did we enter the current state?
	inline GameNetworkingMicroseconds GetTimeEnteredConnectionState() const { return m_usecWhenEnteredConnectionState; }

	/// Fill in connection details
	void ConnectionPopulateInfo( GameNetConnectionInfo_t &info ) const;

//
// Lifetime management
//

	/// Schedule destruction at the next possible opportunity
	void ConnectionQueueDestroy();
	static void ProcessDeletionList();

	/// Free up all resources.  Close sockets, etc
	virtual void FreeResources();

	/// Nuke all transports
	virtual void DestroyTransport();

//
// Connection state machine
// Functions to transition to the specified state.
//

	void ConnectionState_ProblemDetectedLocally( EGameNetConnectionEnd eReason, PRINTF_FORMAT_STRING const char *pszFmt, ... ) FMTFUNCTION( 3, 4 );
	void ConnectionState_ClosedByPeer( int nReason, const char *pszDebug );
	void ConnectionState_FindingRoute( GameNetworkingMicroseconds usecNow );
	bool BConnectionState_Connecting( GameNetworkingMicroseconds usecNow, GameNetworkingErrMsg &errMsg );
	void ConnectionState_Connected( GameNetworkingMicroseconds usecNow );
	void ConnectionState_FinWait();

//
// Misc internal stuff
//

	/// What interface is responsible for this connection?
	CGameNetworkingSockets *const m_pGameNetworkingSocketsInterface;

	/// Current active transport for this connection.
	/// MIGHT BE NULL in certain failure / edge cases!
	/// Might change during the connection lifetime.
	CConnectionTransport *m_pTransport;

	/// Our public handle
	HGameNetConnection m_hConnectionSelf;

	/// Who is on the other end?  This might be invalid if we don't know yet.  (E.g. direct UDP connections.)
	GameNetworkingIdentity m_identityRemote;

	/// Who are we?
	GameNetworkingIdentity m_identityLocal;

	/// The listen socket through which we were accepted, if any.
	CGameNetworkListenSocketBase *m_pParentListenSocket;

	/// What poll group are we assigned to?
	CGameNetworkPollGroup *m_pPollGroup;

	/// Assign poll group
	void SetPollGroup( CGameNetworkPollGroup *pPollGroup );

	/// Remove us from the poll group we are in (if any)
	void RemoveFromPollGroup();

	/// Was this connection initiated locally (we are the "client") or remotely (we are the "server")?
	/// In *most* use cases, "server" connections have a listen socket, but not always.
	bool m_bConnectionInitiatedRemotely;

	/// Our handle in our parent's m_listAcceptedConnections (if we were accepted on a listen socket)
	int m_hSelfInParentListenSocketMap;

	// Linked list of received messages
	GameNetworkingMessageQueue m_queueRecvMessages;

	/// The unique 64-bit end-to-end connection ID.  Each side picks 32 bits
	uint32 m_unConnectionIDLocal;
	uint32 m_unConnectionIDRemote;

	/// Track end-to-end stats for this connection.
	LinkStatsTracker<LinkStatsTrackerEndToEnd> m_statsEndToEnd;

	/// When we accept a connection, they will send us a timestamp we should send back
	/// to them, so that they can estimate the ping
	uint64 m_ulHandshakeRemoteTimestamp;
	GameNetworkingMicroseconds m_usecWhenReceivedHandshakeRemoteTimestamp;

	/// Connection configuration
	ConnectionConfig m_connectionConfig;

	/// The reason code for why the connection was closed.
	EGameNetConnectionEnd m_eEndReason;
	ConnectionEndDebugMsg m_szEndDebug;

	/// MTU values for this connection
	int m_cbMTUPacketSize = 0;
	int m_cbMaxPlaintextPayloadSend = 0;
	int m_cbMaxMessageNoFragment = 0;
	int m_cbMaxReliableMessageSegment = 0;

	void UpdateMTUFromConfig();

	// Each connection is protected by a lock.  The actual lock to use is IThinker::m_pLock.
	// Almost all connections use this default lock.  (A few special cases use a different lock
	// so that they are locked at the same time as other objects.)
	ConnectionLock m_defaultLock;
	void _AssertLocksHeldByCurrentThread( const char *pszFile, int line, const char *pszTag = nullptr ) const
	{
		GameNetworkingGlobalLock::_AssertHeldByCurrentThread( pszFile, line, pszTag );
		m_pLock->_AssertHeldByCurrentThread( pszFile, line );
	}

	/// Expand the packet number, and decrypt the data chunk.
	/// Returns true if everything is OK and we should continue
	/// processing the packet
	bool DecryptDataChunk( uint16 nWireSeqNum, int cbPacketSize, const void *pChunk, int cbChunk, RecvPacketContext_t &ctx );

	/// Decode the plaintext.  Returns false if the packet seems corrupt or bogus, or should abort further
	/// processing.
	bool ProcessPlainTextDataChunk( int usecTimeSinceLast, RecvPacketContext_t &ctx );

	/// Called when we receive an (end-to-end) packet with a sequence number
	void RecvNonDataSequencedPacket( int64 nPktNum, GameNetworkingMicroseconds usecNow );

	// Called from SNP to update transmit/receive speeds
	void UpdateSpeeds( int nTXSpeed, int nRXSpeed );

	/// Called when the async process to request a cert has failed.
	void CertRequestFailed( EGameNetConnectionEnd nConnectionEndReason, const char *pszMsg );
	bool BHasLocalCert() const { return m_msgSignedCertLocal.has_cert(); }
	void SetLocalCert( const CMsgSteamDatagramCertificateSigned &msgSignedCert, const CECSigningPrivateKey &keyPrivate, bool bCertHasIdentity );
	void InterfaceGotCert();

	bool SNP_BHasAnyBufferedRecvData() const
	{
		return !m_receiverState.m_bufReliableStream.empty();
	}
	bool SNP_BHasAnyUnackedSentReliableData() const
	{
		return m_senderState.m_cbPendingReliable > 0 || m_senderState.m_cbSentUnackedReliable > 0;
	}

	/// Return true if we have any reason to send a packet.  This doesn't mean we have the bandwidth
	/// to send it now, it just means we would like to send something ASAP
	inline bool SNP_WantsToSendPacket() const
	{
		return m_receiverState.TimeWhenFlushAcks() < INT64_MAX || SNP_TimeWhenWantToSendNextPacket() < INT64_MAX;
	}

	/// Send a data packet now, even if we don't have the bandwidth available.  Returns true if a packet was
	/// sent successfully, false if there was a problem.  This will call SendEncryptedDataChunk to do the work
	bool SNP_SendPacket( CConnectionTransport *pTransport, SendPacketContext_t &ctx );

	/// Record that we sent a non-data packet.  This is so that if the peer acks,
	/// we can record it as a ping
	void SNP_SentNonDataPacket( CConnectionTransport *pTransport, int cbPkt, GameNetworkingMicroseconds usecNow );

	/// Called after the connection state changes.  Default behavior is to notify
	/// the active transport, if any
	virtual void ConnectionStateChanged( EGameNetworkingConnectionState eOldState );

	/// Called to post a callback
	int m_nSupressStateChangeCallbacks;
	void PostConnectionStateChangedCallback( EGameNetworkingConnectionState eOldAPIState, EGameNetworkingConnectionState eNewAPIState );

	void QueueEndToEndAck( bool bImmediate, GameNetworkingMicroseconds usecNow )
	{
		if ( bImmediate )
		{
			m_receiverState.QueueFlushAllAcks( k_nThinkTime_ASAP );
			SetNextThinkTimeASAP();
		}
		else
		{
			QueueFlushAllAcks( usecNow + k_usecMaxDataAckDelay );
		}
	}

	void QueueFlushAllAcks( GameNetworkingMicroseconds usecWhen )
	{
		m_receiverState.QueueFlushAllAcks( usecWhen );
		EnsureMinThinkTime( m_receiverState.TimeWhenFlushAcks() );
	}

	inline const CMsgSteamDatagramSessionCryptInfoSigned &GetSignedCryptLocal() { return m_msgSignedCryptLocal; }
	inline const CMsgSteamDatagramCertificateSigned &GetSignedCertLocal() { return m_msgSignedCertLocal; }
	inline bool BCertHasIdentity() const { return m_bCertHasIdentity; }
	inline bool BCryptKeysValid() const { return m_bCryptKeysValid; }

	/// Called when we send an end-to-end connect request
	void SentEndToEndConnectRequest( GameNetworkingMicroseconds usecNow )
	{

		// Reset timeout/retry for this reply.  But if it fails, we'll start
		// the whole handshake over again.  It keeps the code simpler, and the
		// challenge value has a relatively short expiry anyway.
		m_usecWhenSentConnectRequest = usecNow;
		EnsureMinThinkTime( usecNow + k_usecConnectRetryInterval );
	}

	/// Symmetric mode
	inline bool BSymmetricMode() const { return m_connectionConfig.m_SymmetricConnect.Get() != 0; }
	virtual bool BSupportsSymmetricMode();

	// Check the certs, save keys, etc
	bool BRecvCryptoHandshake( const CMsgSteamDatagramCertificateSigned &msgCert, const CMsgSteamDatagramSessionCryptInfoSigned &msgSessionInfo, bool bServer );
	bool BFinishCryptoHandshake( bool bServer );

	/// Check state of connection.  Check for timeouts, and schedule time when we
	/// should think next
	void CheckConnectionStateAndSetNextThinkTime( GameNetworkingMicroseconds usecNow );

	/// Same as CheckConnectionStateAndSetNextThinkTime, but can be called when we don't
	/// already have the global lock.  If we can take the necessary locks now, without
	/// blocking, then we'll go ahead and take action now.  If we cannot, we will
	/// just schedule a wakeup call
	void CheckConnectionStateOrScheduleWakeUp( GameNetworkingMicroseconds usecNow );

	// Upcasts.  So we don't have to compile with RTTI
	virtual CGameNetworkConnectionP2P *AsGameNetworkConnectionP2P();

	/// Check if this connection is an internal connection for the
	/// ISteamMessages interface.  The messages layer *mostly* works
	/// on top of the sockets system, but in a few places we need
	/// to break the abstraction and do things other clients of the
	/// API could not do easily
	inline bool IsConnectionForMessagesSession() const { return m_connectionConfig.m_LocalVirtualPort.Get() == k_nVirtualPort_Messages; }

protected:
	CGameNetworkConnectionBase( CGameNetworkingSockets *pGameNetworkingSocketsInterface, ConnectionScopeLock &scopeLock );
	virtual ~CGameNetworkConnectionBase(); // hidden destructor, don't call directly.  Use ConnectionQueueDestroy()

	/// Initialize connection bookkeeping
	bool BInitConnection( GameNetworkingMicroseconds usecNow, int nOptions, const GameNetworkingConfigValue_t *pOptions, SteamDatagramErrMsg &errMsg );

	/// Called from BInitConnection, to start obtaining certs, etc
	virtual void InitConnectionCrypto( GameNetworkingMicroseconds usecNow );

	/// Name assigned by app (for debugging)
	char m_szAppName[ k_cchGameNetworkingMaxConnectionDescription ];

	/// More complete debug description (for debugging)
	char m_szDescription[ k_cchGameNetworkingMaxConnectionDescription ];

	/// Set the connection description.  Should include the connection type and peer address.
	virtual void GetConnectionTypeDescription( ConnectionTypeDescription_t &szDescription ) const = 0;

	/// Misc periodic processing.
	/// Called from within CheckConnectionStateAndSetNextThinkTime.
	/// Will be called in any connection state.
	virtual void ThinkConnection( GameNetworkingMicroseconds usecNow );

	/// Called from the connection Think() state machine, for connections that have been
	/// initiated locally and that are in the connecting state.
	///
	/// Should return the next time when it needs to be woken up.  Or it can set the next
	/// think time directly, if it is awkward to return.  That is slightly
	/// less efficient.
	///
	/// Base class sends connect requests (including periodic retry) through the current
	/// transport.
	virtual GameNetworkingMicroseconds ThinkConnection_ClientConnecting( GameNetworkingMicroseconds usecNow );

	/// Called from the connection Think() state machine, when the connection is in the finding
	/// route state.  The connection should return the next time when it needs to be woken up.
	/// Or it can set the next think time directly, if it is awkward to return.  That is slightly
	/// less efficient.
	virtual GameNetworkingMicroseconds ThinkConnection_FindingRoute( GameNetworkingMicroseconds usecNow );

	/// Called when a timeout is detected
	void ConnectionTimedOut( GameNetworkingMicroseconds usecNow );

	/// Called when a timeout is detected to tried to provide a more specific error
	/// message.
	virtual void ConnectionGuessTimeoutReason( EGameNetConnectionEnd &nReasonCode, ConnectionEndDebugMsg &msg, GameNetworkingMicroseconds usecNow );

	/// Called when we receive a complete message.  Should allocate a message object and put it into the proper queues
	bool ReceivedMessage( const void *pData, int cbData, int64 nMsgNum, int nFlags, GameNetworkingMicroseconds usecNow );
	void ReceivedMessage( CGameNetworkingMessage *pMsg );

	/// Timestamp when we last sent an end-to-end connection request packet
	GameNetworkingMicroseconds m_usecWhenSentConnectRequest;

	//
	// Crypto
	//

	void ClearCrypto();
	bool BThinkCryptoReady( GameNetworkingMicroseconds usecNow );
	void SetLocalCertUnsigned();
	void ClearLocalCrypto();
	void FinalizeLocalCrypto();
	void SetCryptoCipherList();

	// Remote cert and crypt info.  We need to hand on to the original serialized version briefly
	std::string m_sCertRemote;
	std::string m_sCryptRemote;
	CMsgSteamDatagramCertificate m_msgCertRemote;
	CMsgSteamDatagramSessionCryptInfo m_msgCryptRemote;

	// Local crypto info for this connection
	CECSigningPrivateKey m_keyPrivate; // Private key corresponding to our cert.  We'll wipe this in FinalizeLocalCrypto, as soon as we've locked in the crypto properties we're going to use
	CECKeyExchangePrivateKey m_keyExchangePrivateKeyLocal;
	CMsgSteamDatagramSessionCryptInfo m_msgCryptLocal;
	CMsgSteamDatagramSessionCryptInfoSigned m_msgSignedCryptLocal;
	CMsgSteamDatagramCertificateSigned m_msgSignedCertLocal;
	bool m_bCertHasIdentity; // Does the cert contain the identity we will use for this connection?
	EGameNetworkingSocketsCipher m_eNegotiatedCipher;

	// AES keys used in each direction
	bool m_bCryptKeysValid;
	AES_GCM_EncryptContext m_cryptContextSend;
	AES_GCM_DecryptContext m_cryptContextRecv;

	// Initialization vector for AES-GCM.  These are combined with
	// the packet number so that the effective IV is unique per
	// packet.  We use a 96-bit IV, which is what TLS uses (RFC5288),
	// what NIST recommends (https://dl.acm.org/citation.cfm?id=2206251),
	// and what makes GCM the most efficient. 
	AutoWipeFixedSizeBuffer<12> m_cryptIVSend;
	AutoWipeFixedSizeBuffer<12> m_cryptIVRecv;

	/// Check if the remote cert (m_msgCertRemote) is acceptable.  If not, return the
	/// appropriate connection code and error message.  If pCACertAuthScope is NULL, the
	/// cert is not signed.  (The base class will check if this is allowed.)  If pCACertAuthScope
	/// is present, the cert was signed and the chain of trust has been verified, and the CA trust
	/// chain has authorized the specified rights.
	virtual EGameNetConnectionEnd CheckRemoteCert( const CertAuthScope *pCACertAuthScope, GameNetworkingErrMsg &errMsg );

	/// Called when we the remote host presents us with an unsigned cert.
	virtual EUnsignedCert AllowRemoteUnsignedCert();

	/// Called to decide if we want to try to proceed without a signed cert for ourselves
	virtual EUnsignedCert AllowLocalUnsignedCert();

	//
	// "SNP" - Steam Networking Protocol.  (Sort of audacious to stake out this acronym, don't you think...?)
	//         The layer that does end-to-end reliability and bandwidth estimation
	//

	void SNP_InitializeConnection( GameNetworkingMicroseconds usecNow );
	void SNP_ShutdownConnection();
	int64 SNP_SendMessage( CGameNetworkingMessage *pSendMessage, GameNetworkingMicroseconds usecNow, bool *pbThinkImmediately );
	GameNetworkingMicroseconds SNP_ThinkSendState( GameNetworkingMicroseconds usecNow );
	GameNetworkingMicroseconds SNP_GetNextThinkTime( GameNetworkingMicroseconds usecNow );
	GameNetworkingMicroseconds SNP_TimeWhenWantToSendNextPacket() const;
	void SNP_PrepareFeedback( GameNetworkingMicroseconds usecNow );
	void SNP_ReceiveUnreliableSegment( int64 nMsgNum, int nOffset, const void *pSegmentData, int cbSegmentSize, bool bLastSegmentInMessage, GameNetworkingMicroseconds usecNow );
	bool SNP_ReceiveReliableSegment( int64 nPktNum, int64 nSegBegin, const uint8 *pSegmentData, int cbSegmentSize, GameNetworkingMicroseconds usecNow );
	int SNP_ClampSendRate();
	void SNP_PopulateDetailedStats( SteamDatagramLinkStats &info );
	void SNP_PopulateQuickStats( GameNetworkingQuickConnectionStatus &info, GameNetworkingMicroseconds usecNow );
	void SNP_RecordReceivedPktNum( int64 nPktNum, GameNetworkingMicroseconds usecNow, bool bScheduleAck );
	EResult SNP_FlushMessage( GameNetworkingMicroseconds usecNow );

	/// Accumulate "tokens" into our bucket base on the current calculated send rate
	void SNP_TokenBucket_Accumulate( GameNetworkingMicroseconds usecNow );

	/// Mark a packet as dropped
	void SNP_SenderProcessPacketNack( int64 nPktNum, SNPInFlightPacket_t &pkt, const char *pszDebug );

	/// Check in flight packets.  Expire any that need to be, and return the time when the
	/// next one that is not yet expired will be expired.
	GameNetworkingMicroseconds SNP_SenderCheckInFlightPackets( GameNetworkingMicroseconds usecNow );

	SSNPSenderState m_senderState;
	SSNPReceiverState m_receiverState;

	/// Bandwidth estimation data
	SSendRateData m_sendRateData; // FIXME Move this to transport!

	/// Called from SNP layer when it decodes a packet that serves as a ping measurement
	virtual void ProcessSNPPing( int msPing, RecvPacketContext_t &ctx );

private:

	void SNP_GatherAckBlocks( SNPAckSerializerHelper &helper, GameNetworkingMicroseconds usecNow );
	uint8 *SNP_SerializeAckBlocks( const SNPAckSerializerHelper &helper, uint8 *pOut, const uint8 *pOutEnd, GameNetworkingMicroseconds usecNow );
	uint8 *SNP_SerializeStopWaitingFrame( uint8 *pOut, const uint8 *pOutEnd, GameNetworkingMicroseconds usecNow );

	void SetState( EGameNetworkingConnectionState eNewState, GameNetworkingMicroseconds usecNow );
	EGameNetworkingConnectionState m_eConnectionState;

	/// State of the connection as our peer would observe it.
	/// (Certain local state transitions are not meaningful.)
	///
	/// Differs from m_eConnectionState in two ways:
	/// - Linger is not used.  Instead, to the peer we are "connected."
	/// - When the local connection state transitions
	///   from ProblemDetectedLocally or ClosedByPeer to FinWait,
	///   when the application closes the connection, this value
	///   will not change.  It will retain the previous state,
	///   so that while we are in the FinWait state, we can send
	///   appropriate cleanup messages.
	EGameNetworkingConnectionState m_eConnectionWireState;

	/// Timestamp when we entered the current state.  Used for various
	/// timeouts.
	GameNetworkingMicroseconds m_usecWhenEnteredConnectionState;

	// !DEBUG! Log of packets we sent.
	#ifdef SNP_ENABLE_PACKETSENDLOG
	struct PacketSendLog
	{
		// State before we sent anything
		GameNetworkingMicroseconds m_usecTime;
		int m_cbPendingReliable;
		int m_cbPendingUnreliable;
		int m_nPacketGaps;
		float m_fltokens;
		int64 m_nPktNumNextPendingAck;
		GameNetworkingMicroseconds m_usecNextPendingAckTime;
		int64 m_nMaxPktRecv;
		int64 m_nMinPktNumToSendAcks;

		int m_nAckBlocksNeeded;

		// What we sent
		int m_nAckBlocksSent;
		int64 m_nAckEnd;
		int m_nReliableSegmentsRetry;
		int m_nSegmentsSent;
		int m_cbSent;
	};
	std_vector<PacketSendLog> m_vecSendLog;
	#endif

	// Implements IThinker.
	// Connections must not override this, or call it directly.
	// Do any periodic work in ThinkConnection()
	virtual void Think( GameNetworkingMicroseconds usecNow ) override final;
};

/// Abstract base class for sending end-to-end data for a connection.
///
/// Many connection classes only have one transport, but some may
/// may have more than one transport, and dynamically switch between
/// them.  (E.g. it will try local LAN, NAT piercing, then fallback to relay)
class CConnectionTransport
{
public:

	/// The connection we were created to service.  A given transport object
	/// is always created for a single connection (and that will not change,
	/// hence this is a reference and not a pointer).  However, a connection may
	/// create more than one transport.
	CGameNetworkConnectionBase &m_connection;

	/// Use this function to actually delete the object.  Do not use operator delete
	void TransportDestroySelfNow();

	/// Free up transport resources.  Called just before destruction.  If you have cleanup
	/// that might involved calling virtual methods, do it in here
	virtual void TransportFreeResources();

	/// Called by SNP pacing layer, when it has some data to send and there is bandwidth available.
	/// The derived class should setup a context, reserving the space it needs, and then call SNP_SendPacket.
	/// Returns true if a packet was sent successfully, false if there was a problem.
	virtual bool SendDataPacket( GameNetworkingMicroseconds usecNow ) = 0;

	/// Connection will call this to ask the transport to surround the
	/// "chunk" with the appropriate framing, and route it to the 
	/// appropriate host.  A "chunk" might contain a mix of reliable 
	/// and unreliable data.  We use the same framing for data 
	/// payloads for all connection types.  Return value is 
	/// the number of bytes written to the network layer, UDP/IP 
	/// header is not included.
	///
	/// ctx is whatever the transport passed to SNP_SendPacket, if the
	/// connection initiated the sending of the packet
	virtual int SendEncryptedDataChunk( const void *pChunk, int cbChunk, SendPacketContext_t &ctx ) = 0;

	/// Return true if we are currently able to send end-to-end messages.
	virtual bool BCanSendEndToEndConnectRequest() const;
	virtual bool BCanSendEndToEndData() const = 0;
	virtual void SendEndToEndConnectRequest( GameNetworkingMicroseconds usecNow );
	virtual void SendEndToEndStatsMsg( EStatsReplyRequest eRequest, GameNetworkingMicroseconds usecNow, const char *pszReason ) = 0;
	virtual void TransportPopulateConnectionInfo( GameNetConnectionInfo_t &info ) const;
	virtual void GetDetailedConnectionStatus( GameNetworkingDetailedConnectionStatus &stats, GameNetworkingMicroseconds usecNow );

	/// Called when the connection state changes.  Some transports need to do stuff
	virtual void TransportConnectionStateChanged( EGameNetworkingConnectionState eOldState );

	/// Called when a timeout is detected to tried to provide a more specific error
	/// message
	virtual void TransportGuessTimeoutReason( EGameNetConnectionEnd &nReasonCode, ConnectionEndDebugMsg &msg, GameNetworkingMicroseconds usecNow );

	// Some accessors for commonly needed info
	inline EGameNetworkingConnectionState ConnectionState() const { return m_connection.GetState(); }
	inline EGameNetworkingConnectionState ConnectionWireState() const { return m_connection.GetWireState(); }
	inline uint32 ConnectionIDLocal() const { return m_connection.m_unConnectionIDLocal; }
	inline uint32 ConnectionIDRemote() const { return m_connection.m_unConnectionIDRemote; }
	inline CGameNetworkListenSocketBase *ListenSocket() const { return m_connection.m_pParentListenSocket; }
	inline const GameNetworkingIdentity &IdentityLocal() const { return m_connection.m_identityLocal; }
	inline const GameNetworkingIdentity &IdentityRemote() const { return m_connection.m_identityRemote; }
	inline const char *ConnectionDescription() const { return m_connection.GetDescription(); }

	void _AssertLocksHeldByCurrentThread( const char *pszFile, int line, const char *pszTag = nullptr ) const
	{
		m_connection._AssertLocksHeldByCurrentThread( pszFile, line, pszTag );
	}

	// Useful so we can use ScheduledMethodThinkerLockable
	bool TryLock() { return m_connection.TryLock(); }
	void Unlock() { m_connection.Unlock(); }

protected:

	inline CConnectionTransport( CGameNetworkConnectionBase &conn ) : m_connection( conn ) {}
	virtual ~CConnectionTransport() {} // Destructor protected -- use TransportDestroySelfNow()
};

// Delayed inline
inline ConnectionScopeLock::ConnectionScopeLock( CGameNetworkConnectionBase &conn, const char *pszTag ) : ScopeLock<ConnectionLock>( *conn.m_pLock, pszTag ) {}
inline void ConnectionScopeLock::Lock( CGameNetworkConnectionBase &conn, const char *pszTag ) { ScopeLock<ConnectionLock>::Lock( *conn.m_pLock, pszTag ); }

/// Dummy loopback/pipe connection that doesn't actually do any network work.
/// For these types of connections, the distinction between connection and transport
/// is not really useful
class CGameNetworkConnectionPipe final : public CGameNetworkConnectionBase, public CConnectionTransport
{
public:

	/// Create a pair of loopback connections that are immediately connected to each other
	/// No callbacks are posted.
	static bool APICreateSocketPair( CGameNetworkingSockets *pGameNetworkingSocketsInterface, CGameNetworkConnectionPipe **pOutConnections, const GameNetworkingIdentity pIdentity[2] );

	/// Create a pair of loopback connections that act like normal connections, but use internal transport.
	/// The two connections will be placed in the "connecting" state, and will go through the ordinary
	/// state machine.
	///
	/// The client connection is returned.
	static CGameNetworkConnectionPipe *CreateLoopbackConnection(
		CGameNetworkingSockets *pClientInstance, int nOptions, const GameNetworkingConfigValue_t *pOptions,
		CGameNetworkListenSocketBase *pListenSocket,
		GameNetworkingErrMsg &errMsg,
		ConnectionScopeLock &scopeLock );

	/// The guy who is on the other end.
	CGameNetworkConnectionPipe *m_pPartner;

	// CGameNetworkConnectionBase overrides
	virtual int64 _APISendMessageToConnection( CGameNetworkingMessage *pMsg, GameNetworkingMicroseconds usecNow, bool *pbThinkImmediately ) override;
	virtual EResult AcceptConnection( GameNetworkingMicroseconds usecNow ) override;
	virtual void InitConnectionCrypto( GameNetworkingMicroseconds usecNow ) override;
	virtual EUnsignedCert AllowRemoteUnsignedCert() override;
	virtual EUnsignedCert AllowLocalUnsignedCert() override;
	virtual void GetConnectionTypeDescription( ConnectionTypeDescription_t &szDescription ) const override;
	virtual void DestroyTransport() override;
	virtual void ConnectionStateChanged( EGameNetworkingConnectionState eOldState ) override;

	// CGameNetworkConnectionTransport
	virtual bool SendDataPacket( GameNetworkingMicroseconds usecNow ) override;
	virtual bool BCanSendEndToEndConnectRequest() const override;
	virtual bool BCanSendEndToEndData() const override;
	virtual void SendEndToEndConnectRequest( GameNetworkingMicroseconds usecNow ) override;
	virtual void SendEndToEndStatsMsg( EStatsReplyRequest eRequest, GameNetworkingMicroseconds usecNow, const char *pszReason ) override;
	virtual int SendEncryptedDataChunk( const void *pChunk, int cbChunk, SendPacketContext_t &ctx ) override;
	virtual void TransportPopulateConnectionInfo( GameNetConnectionInfo_t &info ) const override;

private:

	// Use CreateSocketPair!
	CGameNetworkConnectionPipe( CGameNetworkingSockets *pGameNetworkingSocketsInterface, const GameNetworkingIdentity &identity, ConnectionScopeLock &scopeLock );
	virtual ~CGameNetworkConnectionPipe();

	/// Setup the server side of a loopback connection
	bool BBeginAccept( CGameNetworkListenSocketBase *pListenSocket, GameNetworkingMicroseconds usecNow, SteamDatagramErrMsg &errMsg );

	/// Act like we sent a sequenced packet
	void FakeSendStats( GameNetworkingMicroseconds usecNow, int cbPktSize );
};

// Had to delay this until CGameNetworkConnectionBase was defined
template<typename TStatsMsg>
inline void SendPacketContext<TStatsMsg>::CalcMaxEncryptedPayloadSize( size_t cbHdrReserve, CGameNetworkConnectionBase *pConnection )
{
	Assert( m_cbTotalSize >= 0 );
	m_cbMaxEncryptedPayload = pConnection->m_cbMTUPacketSize - (int)cbHdrReserve - m_cbTotalSize;
	Assert( m_cbMaxEncryptedPayload >= 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
// Misc globals
//
/////////////////////////////////////////////////////////////////////////////

extern CUtlHashMap<uint16, CGameNetworkConnectionBase *, std::equal_to<uint16>, Identity<uint16> > g_mapConnections;
extern CUtlHashMap<int, CGameNetworkPollGroup *, std::equal_to<int>, Identity<int> > g_mapPollGroups;

// All of the tables above are projected by the same lock, since we expect to only access it briefly
struct TableLock : Lock<RecursiveMutexImpl> {
	TableLock() : Lock<RecursiveMutexImpl>( "table", LockDebugInfo::k_nFlag_Table ) {}
}; 
using TableScopeLock = ScopeLock<TableLock>;
extern TableLock g_tables_lock;

// This table is protected by the global lock
extern CUtlHashMap<int, CGameNetworkListenSocketBase *, std::equal_to<int>, Identity<int> > g_mapListenSockets;

extern bool BCheckGlobalSpamReplyRateLimit( GameNetworkingMicroseconds usecNow );
extern CGameNetworkConnectionBase *GetConnectionByHandle( HGameNetConnection sock, ConnectionScopeLock &scopeLock );
extern CGameNetworkPollGroup *GetPollGroupByHandle( HGameNetPollGroup hPollGroup, PollGroupScopeLock &scopeLock, const char *pszLockTag );

inline CGameNetworkConnectionBase *FindConnectionByLocalID( uint32 nLocalConnectionID, ConnectionScopeLock &scopeLock )
{
	// We use the wire connection ID as the API handle, so these two operations
	// are currently the same.
	return GetConnectionByHandle( HGameNetConnection( nLocalConnectionID ), scopeLock );
}

} // namespace GameNetworkingSocketsLib

#endif // STEAMNETWORKINGSOCKETS_CONNECTIONS_H

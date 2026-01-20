#pragma once
#include "GameNetworkingSockets.hpp"
#include <pch.hpp>
enum class ConnectionKind : uint8_t {
    eInternal,  // Cluster connections (God, Partition, GameServer, etc.)
    eExternal   // External clients (Unity, remote dashboards, etc.)
};

enum class InterlinkType
{
	eInvalid = 0,
	eShard = 1,	 // partition Server
	eWatchDog = 2,		 // God
	eCartograph = 3,	 // God Debug Tool
	eGameClient = 4, // Game Client / Player
	eGameServer = 5,	 // Game Server
  eProxy = 6,
  eGameCoordinator = 7
};
BOOST_DESCRIBE_ENUM(InterlinkType, eInvalid, eShard, eWatchDog, eCartograph, eGameClient, eGameServer, eProxy, eGameCoordinator)

enum class ConnectionState
{
	eInvalid,		/// Invalid state
	ePreConnecting, /// The Connection request has not been sent yet
	eConnecting,	/// This Connection is trying to connect
	eConnected,		/// Successfully Connected
	eDisconnecting, /// Trying to disconnect
	eClosed,
	eError
};
BOOST_DESCRIBE_ENUM(ConnectionState, eInvalid, ePreConnecting, eConnecting, eConnected, eDisconnecting, eClosed,eError)


enum class InterlinkMessageSendFlag
{
	eImmidiateOrDrop = k_nSteamNetworkingSend_NoDelay, /// Send it NOW or drop it. extremely
													   /// unreliable but quickest. no batching

	eUnreliableNow =
		k_nSteamNetworkingSend_UnreliableNoNagle, /// Send it unreliable as soon as possible.use
												  /// only if you know that this message will be the
												  /// last for a while

	eUnreliableBatched = k_nSteamNetworkingSend_Unreliable, /// Send it unreliably but batch to
															/// other outgoing messages

	eReliableNow =
		k_nSteamNetworkingSend_ReliableNoNagle, /// Send it reliable as soon as possible.use only if
												/// you know that this message will be the last for
												/// a while

	eReliableBatched =
		k_nSteamNetworkingSend_Reliable, /// Send it reliably but batch to other outgoing messages
};
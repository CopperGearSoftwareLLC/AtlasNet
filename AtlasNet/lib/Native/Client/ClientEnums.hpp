#pragma once

#include <boost/describe/enum.hpp>
enum class ClientState
{
	eInvalid,
	eNone,		 // No stage
	eConnected,	 // client connected to a proxy but not assigned a shard
	eSpawned	 // Client connected to proxy and spawned as a AtlasEntity in a shard
};
BOOST_DESCRIBE_ENUM(ClientState, eNone, eConnected, eSpawned)
enum class ClientTransferStage	// A = From Shard, B = To Shard
{
	eNone,				  // Nothing done yet
	eShardPrepare,		  // A -> B notify of intent to transfer
	eShardReady,		  // B -> A notifying readiness to receive transfer,
	eProxyRequestSwitch,  // A -> Proxy request
	eProxyFreeze,		  // Proxy -> A  Notification of request approval and stream freeze
	eShardDrained,		  // A -> Proxy notification of completion on processing lingering packets
	eProxyTransferActivate,	 // Proxy -> B, Transfer complete, ownership transferred
};
BOOST_DESCRIBE_ENUM(ClientTransferStage, eNone, eShardPrepare, eShardReady, eProxyRequestSwitch,
					eProxyFreeze, eShardDrained, eProxyTransferActivate);

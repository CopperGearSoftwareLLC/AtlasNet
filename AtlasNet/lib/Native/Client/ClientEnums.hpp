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
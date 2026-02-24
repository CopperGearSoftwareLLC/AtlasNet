#pragma once

#include <boost/describe/enum.hpp>
enum class EntityTransferStage
{
	eNone,		// Nothing done yet
	ePrepare,	// A -> B notify to prepare to receive certain entities
	eReady,		// B -> A acknowledged
	eCommit,	// A -> B //Freeze simulation and remote calls, sends last snapshot to B
	eComplete,	// B -> A acknowledged, transfer complete

};
BOOST_DESCRIBE_ENUM(EntityTransferStage, eNone, ePrepare, eReady, eCommit, eComplete);
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

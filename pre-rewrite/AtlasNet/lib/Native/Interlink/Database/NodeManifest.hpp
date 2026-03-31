#pragma once

#include <Global/pch.hpp>

#include "Global/Misc/Singleton.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"

struct NodeManifestEntry
{
	std::string nodeName;
	std::string podName;
	std::string podIP;
};

class NodeManifest : public Singleton<NodeManifest>
{
	const static inline std::string HashTableNameShardNode = "Node Manifest Shard_Node";
	static std::string GetKeyOfIdentifier(const NetworkIdentity& ID);

   public:
	NodeManifest();

	void RegisterShardNode(const NetworkIdentity& shardID, const NodeManifestEntry& entry);
	void DeregisterShard(const NetworkIdentity& shardID);
	std::optional<NodeManifestEntry> GetShardNode(const NetworkIdentity& shardID);
	std::unordered_map<NetworkIdentity, NodeManifestEntry> GetAllShardNodes();
	void ClearAll();
};

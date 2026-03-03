#include "NodeManifest.hpp"

#include "Global/Misc/String_utils.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Debug/Log.hpp"

namespace
{
std::string SerializeNodeManifestEntry(const NodeManifestEntry& entry)
{
	Json payload = Json::object();
	payload["nodeName"] = entry.nodeName;
	payload["podName"] = entry.podName;
	payload["podIp"] = entry.podIP;
	return payload.dump();
}

std::optional<NodeManifestEntry> ParseNodeManifestEntry(const std::string& raw)
{
	const Json payload = Json::parse(raw, nullptr, false);
	if (payload.is_discarded() || !payload.is_object())
	{
		return std::nullopt;
	}

	NodeManifestEntry entry;
	if (payload.contains("nodeName") && payload["nodeName"].is_string())
	{
		entry.nodeName = payload["nodeName"].get<std::string>();
	}
	if (payload.contains("podName") && payload["podName"].is_string())
	{
		entry.podName = payload["podName"].get<std::string>();
	}
	if (payload.contains("podIp") && payload["podIp"].is_string())
	{
		entry.podIP = payload["podIp"].get<std::string>();
	}

	if (entry.nodeName.empty() && entry.podName.empty() && entry.podIP.empty())
	{
		return std::nullopt;
	}
	return entry;
}
}  // namespace

void NodeManifest::RegisterShardNode(const NetworkIdentity& shardID,
									 const NodeManifestEntry& entry)
{
	Log logger("NodeManifest");
	logger.DebugFormatted("RegisterShardNode: {} nodeName={} podName={} podIP={}",
						  shardID.ToString(),
						  entry.nodeName,
						  entry.podName,
						  entry.podIP);
	InternalDB::Get()->HSet(HashTableNameShardNode, GetKeyOfIdentifier(shardID),
							NukeString(SerializeNodeManifestEntry(entry)));
}

void NodeManifest::DeregisterShard(const NetworkIdentity& shardID)
{
	Log logger("NodeManifest");
	logger.DebugFormatted("DeregisterShard: {}", shardID.ToString());
	InternalDB::Get()->HDel(HashTableNameShardNode, {GetKeyOfIdentifier(shardID)});
}

std::optional<NodeManifestEntry> NodeManifest::GetShardNode(const NetworkIdentity& shardID)
{
	Log logger("NodeManifest");
	logger.DebugFormatted("GetShardNode: {}", shardID.ToString());
	const auto ret = InternalDB::Get()->HGet(HashTableNameShardNode, GetKeyOfIdentifier(shardID));
	if (!ret.has_value() || ret->empty())
	{
		logger.WarningFormatted("GetShardNode: no entry found for {}", shardID.ToString());
		return std::nullopt;
	}
	return ParseNodeManifestEntry(*ret);
}

std::unordered_map<NetworkIdentity, NodeManifestEntry> NodeManifest::GetAllShardNodes()
{
	Log logger("NodeManifest");
	std::unordered_map<NetworkIdentity, NodeManifestEntry> out;
	const auto entries = InternalDB::Get()->HGetAll(HashTableNameShardNode);
	logger.DebugFormatted("GetAllShardNodes: {} raw entries", entries.size());

	for (const auto& rawEntry : entries)
	{
		NetworkIdentity shardID;
		ByteReader br(rawEntry.first);
		shardID.Deserialize(br);
		if (shardID.Type != NetworkIdentityType::eShard)
		{
			logger.Warning("GetAllShardNodes: skipping non-shard identity");
			continue;
		}

		const auto parsed = ParseNodeManifestEntry(rawEntry.second);
		if (!parsed.has_value())
		{
			logger.Warning("GetAllShardNodes: failed to parse NodeManifestEntry; skipping");
			continue;
		}

		out.insert_or_assign(shardID, *parsed);
	}
	return out;
}

void NodeManifest::ClearAll()
{
	Log logger("NodeManifest");
	logger.Debug("ClearAll called; deleting all NodeManifest keys");
	InternalDB::Get()->DelKey(HashTableNameShardNode);
}

NodeManifest::NodeManifest() {}

std::string NodeManifest::GetKeyOfIdentifier(const NetworkIdentity& ID)
{
	ByteWriter bw;
	ID.Serialize(bw);
	return std::string(bw.as_string_view());
}

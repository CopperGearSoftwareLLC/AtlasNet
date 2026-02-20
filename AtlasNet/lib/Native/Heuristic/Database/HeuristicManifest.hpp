#pragma once

#include <sw/redis++/redis.h>

#include <array>
#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"

class HeuristicManifest : public Singleton<HeuristicManifest>
{
	Log logger = Log("HeuristicManifest");
	/*
	All available bounds are placed in pending.
	At first or on change. each Shard goes and Pops from the pending list.
	Adding it to Claimed with their key.
	*/
	struct PendingBoundStruct
	{
		IBounds::BoundsID ID;
		std::string BoundsDataBase64;
		void to_json(Json& j) const
		{
			j["ID"] = ID;
			j["BoundsData64"] = BoundsDataBase64;
		}
		void from_json(const Json& j)
		{
			ID = j["ID"];
			BoundsDataBase64 = j["BoundsData64"];
		}
	};
	struct ClaimedBoundStruct
	{
		IBounds::BoundsID ID;
		NetworkIdentity identity;
		std::string BoundsDataBase64;
		void to_json(Json& j) const
		{
			j["ID"] = ID;
			ByteWriter bw;
			identity.Serialize(bw);
			j["Owner64"] = bw.as_string_base_64();
			j["OwnerName"] = identity.ToString();
			j["BoundsData64"] = BoundsDataBase64;
		}
		void from_json(const Json& j)
		{
			ID = j["ID"];
			std::string base64IdentityData = j["Owner64"].get<std::string>();
			ByteReader br(base64IdentityData, true);
			identity.Deserialize(br);
			BoundsDataBase64 = j["BoundsData64"];
		}
	};

	const std::string HeuristicTypeKey = "Heuristic_Type";
	const std::string HeuristicSerializeDataKey = "Heuristic_Data";
	/*stores a list of available, unclaimed bounds*/
	/*the bounds id is the shape*/
	// Shared hash tag keeps pending/claimed in the same Redis Cluster slot.
	const std::string PendingHashTable = "{Heuristic_Bounds}:Pending";
	const std::string JSONDataTable = "HeuristicManifest";
	const std::string JSONPendingEntry = "Pending";
	const std::string JSONClaimedEntry = "Claimed";
	const std::string JSONHeuristicData64Entry = "HeuristicData64";

	/*HashTable of claimed Bounds*/
	const std::string ClaimedTableJson = "{Heuristic_Bounds}:Claimed";
	const std::string ClaimedHashTableNID2BoundData =
		"{Heuristic_Bounds}:Claimed_NID2BID";
	const std::string ClaimedHashTableBoundID2NID =
		"{Heuristic_Bounds}:Claimed_BID2NID";

	IHeuristic::Type ActiveHeuristic = IHeuristic::Type::eNone;
	std::shared_ptr<IHeuristic> Heuristic;

	void Internal_InsertPendingBound(const PendingBoundStruct& p);
	void Internal_InsertClaimedBound(const ClaimedBoundStruct& p);
	ClaimedBoundStruct Internal_PullClaimedBound(IBounds::BoundsID id);

   public:
	[[nodiscard]] IHeuristic::Type GetActiveHeuristicType() const;
	void SetActiveHeuristicType(IHeuristic::Type type);

	template <typename BoundType, typename KeyType = std::string>
	void GetAllPendingBounds(std::vector<BoundType>& out_bounds);

	template <typename BoundType, typename KeyType = std::string>
	void StorePendingBounds(const std::vector<BoundType>& in_bounds);

	template <typename BoundType>
	bool ClaimNextPendingBound(const NetworkIdentity& claim_key,
							   BoundType& out_bound);

	void StorePendingBoundsFromByteWriters(
		const std::unordered_map<IBounds::BoundsID, ByteWriter>& in_writers);

	void GetPendingBoundsAsByteReaders(
		std::vector<std::string>& data_for_readers,
		std::unordered_map<IBounds::BoundsID, ByteReader>& brs);

	void GetClaimedBoundsAsByteReaders(
		std::vector<std::string>& data_for_readers,
		std::unordered_map<NetworkIdentity,
						   std::pair<IBounds::BoundsID, ByteReader>>& brs);

	[[nodiscard]] long long GetPendingBoundsCount() const;

	[[nodiscard]] long long GetClaimedBoundsCount() const;

	bool RequeueClaimedBound(const std::string_view& claim_key);

	template <typename BoundType>
	void GetAllClaimedBounds(
		std::unordered_map<NetworkIdentity, BoundType>& out_bounds);

	[[nodiscard]] std::unique_ptr<IHeuristic> PullHeuristic();
	void PushHeuristic(const IHeuristic& h);
	[[nodiscard]] std::optional<NetworkIdentity> ShardFromPosition(
		const Transform& t);

	[[nodiscard]] std::optional<NetworkIdentity> ShardFromBoundID(
		const IBounds::BoundsID id);
};
template <typename BoundType, typename KeyType>
inline void HeuristicManifest::GetAllPendingBounds(
	std::vector<BoundType>& out_bounds)
{
	out_bounds.clear();
	std::vector<std::string> data_for_readers;
	std::unordered_map<IBounds::BoundsID, ByteReader> brs;
	GetPendingBoundsAsByteReaders(data_for_readers, brs);
	const auto PendingBoundsSet = InternalDB::Get()->HGetAll(PendingHashTable);
	out_bounds.reserve(PendingBoundsSet.size());
	for (auto& [bound_id, boundByteReader] : brs)
	{
		out_bounds.emplace_back(BoundType());
		BoundType& b = out_bounds.back();
		b.Deserialize(boundByteReader);
	}
}
template <typename BoundType, typename KeyType>
void HeuristicManifest::StorePendingBounds(
	const std::vector<BoundType>& in_bounds)
{
	// std::string_view s_b, s_id;
	for (int i = 0; i < in_bounds.size(); i++)
	{
		const auto& bounds = in_bounds[i];

		ByteWriter bw;
		bounds.Serialize(bw);
		// s_b = bw.as_string_view();

		PendingBoundStruct p;
		p.ID = bounds.GetID();
		p.BoundsDataBase64 = bw.as_string_base_64();
		Internal_InsertPendingBound(p);
	}
}

template <typename BoundType>
bool HeuristicManifest::ClaimNextPendingBound(const NetworkIdentity& claim_key,
											  BoundType& out_bound)
{
	/**
	 * @brief Atomically claim one field/value from a pending hash into a
	 * claimed hash.
	 * @details In Redis Cluster, both keys must share the same hash slot (use a
	 * hash tag).
	 * @return std::optional<std::string> with claimed value, or empty if no
	 * pending entries exist.
	 static const char* kLuaScript = R"lua(
local pending = KEYS[1]
local claimed = KEYS[2]
local claim_field = ARGV[1]

local existing = redis.call('HGET', claimed, claim_field)
if existing then
  return existing
end

local cursor = '0'
repeat
  local scan = redis.call('HSCAN', pending, cursor, 'COUNT', 1)
  cursor = scan[1]
  local entries = scan[2]
  if #entries > 0 then
	local field = entries[1]
	local value = entries[2]
	redis.call('HDEL', pending, field)
	redis.call('HSET', claimed, claim_field, value)
	return value
  end
until cursor == '0'

return false
)lua";
	 */
	static const char* kLuaScript = R"lua(
-- KEYS[1] = JSON key
-- ARGV[1] = Pending field name
-- ARGV[2] = Claimed field name
-- ARGV[3] = Owner Base64
-- ARGV[4] = Owner Name

local key = KEYS[1]
local pending_field = ARGV[1]
local claimed_field = ARGV[2]
local owner_base64 = ARGV[3]
local owner_name = ARGV[4]

-- Ensure Claimed object exists
local claimed_type = redis.call("JSON.TYPE", key, claimed_field)
if claimed_type == false or claimed_type[1] == nil then
    redis.call("JSON.SET", key, claimed_field, "{}","NX")
end

-- Get all keys in Pending
local pending_keys = redis.call("JSON.OBJKEYS", key, pending_field)

if not pending_keys or #pending_keys == 0 then
    return nil
end

-- Pick the first key (any entry)
local id = tostring(pending_keys[1])  -- <-- force string

-- Get the entry directly
local entry = redis.call("JSON.GET", key, pending_field .. "." .. id)

-- Set it in Claimed
redis.call("JSON.SET", key, claimed_field .. "." .. id, entry)

-- Add Owner and OwnerName fields
redis.call("JSON.SET", key, claimed_field .. "." .. id .. ".Owner64", "\"" .. owner_base64 .. "\"")
redis.call("JSON.SET", key, claimed_field .. "." .. id .. ".OwnerName", "\"" .. owner_name .. "\"")

-- Remove from Pending
redis.call("JSON.DEL", key, pending_field .. "." .. id)

-- Return the claimed entry
return redis.call("JSON.GET", key, claimed_field .. "." .. id)

)lua";
	ByteWriter bw;
	claim_key.Serialize(bw);
	const auto claimed = InternalDB::Get()->WithSync(
		[&](auto& r) -> auto
		{
			const auto result = r.template command<std::optional<std::string>>(
				"EVAL", kLuaScript, "1", JSONDataTable, JSONPendingEntry,
				JSONClaimedEntry, bw.as_string_base_64(), claim_key.ToString());
			return result;
		});

	if (!claimed)
		return false;

	ByteReader br(claimed.value());
	out_bound.Deserialize(br);
	return true;
}

template <typename BoundType>
void HeuristicManifest::GetAllClaimedBounds(
	std::unordered_map<NetworkIdentity, BoundType>& out_bounds)
{
	out_bounds.clear();
	const auto BoundsTable =
		InternalDB::Get()->HGetAll(ClaimedHashTableNID2BoundData);
	out_bounds.reserve(BoundsTable.size());

	for (const auto& [key, boundsString] : BoundsTable)
	{
		NetworkIdentity id;
		ByteReader brKey(key);
		id.Deserialize(brKey);
		auto [it, inserted] = out_bounds.emplace(id, BoundType());
		BoundType& b = it->second;
		ByteReader br(boundsString);
		b.Deserialize(br);
	}
}
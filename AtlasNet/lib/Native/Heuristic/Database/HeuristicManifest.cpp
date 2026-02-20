#include "HeuristicManifest.hpp"

#include <sw/redis++/redis.h>

#include <boost/describe/enum_from_string.hpp>
#include <memory>
#include <unordered_map>

#include "Global/Serialize/ByteReader.hpp"
#include "Global/pch.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"
// IHeuristic::Type HeuristicManifest::GetActiveHeuristic() const
//{
//	const auto TypeEntry = InternalDB::Get()->Get(HeuristicTypeKey);
//	if (!TypeEntry) //if the entry in redis does not exist the its none of em.
//		return IHeuristic::Type::eNone;
//
//	IHeuristic::Type type;
//
//	boost::describe::enum_from_string(TypeEntry.value(), type);
//	return type;
// }
void HeuristicManifest::GetPendingBoundsAsByteReaders(
	std::vector<std::string>& data_for_readers,
	std::unordered_map<IBounds::BoundsID, ByteReader>& brs)
{
	const auto values = InternalDB::Get()->HGetAll(PendingHashTable);
	for (const auto& [key_raw, value_raw] : values)
	{
		data_for_readers.push_back(value_raw);
		ByteReader key_reader(key_raw);
		const auto ID = key_reader.read_scalar<IBounds::BoundsID>();
		brs.emplace(ID, ByteReader(data_for_readers.back()));
	}
}
void HeuristicManifest::StorePendingBoundsFromByteWriters(
	const std::unordered_map<IBounds::BoundsID, ByteWriter>& in_writers)
{
	std::string_view s_id;
	for (const auto& [ID, writer] : in_writers)
	{
		ByteWriter bw_id;
		bw_id.u32(ID);
		s_id = bw_id.as_string_view();
		PendingBoundStruct p{
			.ID = ID,
			.BoundsDataBase64 = std::string(writer.as_string_base_64())};
		Internal_InsertPendingBound(p);
	}
}
long long HeuristicManifest::GetPendingBoundsCount() const
{
	return InternalDB::Get()->HLen(PendingHashTable);
}
long long HeuristicManifest::GetClaimedBoundsCount() const
{
	return InternalDB::Get()->HLen(ClaimedHashTableNID2BoundData);
}

bool HeuristicManifest::RequeueClaimedBound(const std::string_view& claim_key)
{
	/**
	 * @brief Atomically move a claimed value back to pending.
	 * @details In Redis Cluster, both keys must share the same hash slot (use a
	 * hash tag).
	 * @return true if a claimed value was requeued, false otherwise.
	 */
	static const char* kLuaScript = R"lua(
local claimed = KEYS[1]
local pending = KEYS[2]
local claim_field = ARGV[1]

local value = redis.call('HGET', claimed, claim_field)
if not value then
  return 0
end
if #value < 4 then
  return 0
end

local b1, b2, b3, b4 = string.byte(value, 1, 4)
local field = string.char(b1, b2, b3, b4)
redis.call('HSET', pending, field, value)
redis.call('HDEL', claimed, claim_field)
return 1
)lua";
	return InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			const auto result = r.template command<long long>(
				"EVAL", kLuaScript, "2", ClaimedHashTableNID2BoundData,
				PendingHashTable, claim_key);
			return result != 0;
		});
}
void HeuristicManifest::SetActiveHeuristicType(IHeuristic::Type type)
{
	const char* str = IHeuristic::TypeToString(type);
	const bool result = InternalDB::Get()->Set(HeuristicTypeKey, str);
	ASSERT(result, "Failed to set key");
}
IHeuristic::Type HeuristicManifest::GetActiveHeuristicType() const
{
	const auto get = InternalDB::Get()->Get(HeuristicTypeKey);
	if (!get)
		return IHeuristic::Type::eNone;
	IHeuristic::Type type;
	IHeuristic::TypeFromString(get.value(), type);
	return type;
}

std::optional<NetworkIdentity> HeuristicManifest::ShardFromPosition(
	const Transform& t)
{
	const auto heuristic = PullHeuristic();
	ASSERT(heuristic, "Pull heuristic returned nothing");
	const auto bound = heuristic->QueryPosition(t.position);
	logger.DebugFormatted("found bound for position {}", bound->GetID());
	return ShardFromBoundID(bound->GetID());
	// bound->GetID()
}
std::optional<NetworkIdentity> HeuristicManifest::ShardFromBoundID(
	const IBounds::BoundsID id)
{
	const ClaimedBoundStruct claimedBound = Internal_PullClaimedBound(id);
	return claimedBound.identity;
}
void HeuristicManifest::GetClaimedBoundsAsByteReaders(
	std::vector<std::string>& data_for_readers,
	std::unordered_map<NetworkIdentity,
					   std::pair<IBounds::BoundsID, ByteReader>>& brs)
{
}
void HeuristicManifest::Internal_InsertPendingBound(const PendingBoundStruct& p)
{
	Json j;
	p.to_json(j);

	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			// 1️⃣ Ensure Pending object exists without overwriting
			std::array<std::string, 5> ensure_pending_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONPendingEntry,	 // .Pending
				"{}",					 // empty object if missing
				"NX"					 // only create if not exists
			};
			r.command(ensure_pending_cmd.begin(), ensure_pending_cmd.end());

			// 2️⃣ Insert/overwrite object by ID
			std::array<std::string, 4> set_object_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONPendingEntry + "." +
					std::to_string(p.ID),  // e.g., .Pending.0
				j.dump()				   // JSON string
			};
			r.command(set_object_cmd.begin(), set_object_cmd.end());
		});
}
void HeuristicManifest::Internal_InsertClaimedBound(const ClaimedBoundStruct& c)
{
	Json j;
	c.to_json(j);

	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			// 1️⃣ Ensure Claimed object exists without overwriting
			std::array<std::string, 5> ensure_claimed_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONClaimedEntry,	 // e.g., .Claimed
				"{}",					 // empty object if missing
				"NX"					 // only create if not exists
			};
			r.command(ensure_claimed_cmd.begin(), ensure_claimed_cmd.end());

			// 2️⃣ Insert/overwrite object by ID
			std::array<std::string, 4> set_object_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONClaimedEntry + "." +
					std::to_string(c.ID),  // e.g., .Claimed.0
				j.dump()				   // JSON string for this claimed entry
			};
			r.command(set_object_cmd.begin(), set_object_cmd.end());
		});
}
void HeuristicManifest::PushHeuristic(const IHeuristic& h)
{
	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			ByteWriter bw;
			h.Serialize(bw);

			{
				const auto sp = bw.bytes();
				for (size_t i = 0; i < sp.size(); ++i)
				{
					// Print each byte as 2-digit hex
					std::cout << std::hex << std::setw(2) << std::setfill('0')
							  << static_cast<int>(sp[i]);

					// Optional: add a space between bytes
					if (i != sp.size() - 1)
						std::cout << " ";
				}
				std::cout << std::dec << "\n";	// reset to decimal
			}

			std::array<std::string, 4> set_root_cmd = {
				"JSON.SET", JSONDataTable, ".",
				std::format(R"({{"{}":"{}"}})", JSONHeuristicData64Entry,
							bw.as_string_base_64())	 // fixed escaping if needed
			};
			r.command(set_root_cmd.begin(), set_root_cmd.end());
		});
}
std::unique_ptr<IHeuristic> HeuristicManifest::PullHeuristic()
{
	std::unique_ptr<IHeuristic> heuristic;
	switch (GetActiveHeuristicType())
	{
		case IHeuristic::Type::eGridCell:
			heuristic = std::make_unique<GridHeuristic>();
			break;
		case IHeuristic::Type::eOctree:
			break;
		case IHeuristic::Type::eQuadtree:
			break;

		default:
		case IHeuristic::Type::eNone:
			throw std::runtime_error("Invalid Heuristic?");
			break;
	}
	std::optional<std::string> serializedData64 = InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string>
		{
			// JSON.GET key .HeuristicData64
			std::array<std::string, 3> get_cmd = {
				"JSON.GET",
				JSONDataTable,	// the Redis key
				"." + JSONHeuristicData64Entry
				// path to the field
			};

			return r.template command<std::optional<std::string>>(
				get_cmd.begin(), get_cmd.end());
		});
	ASSERT(serializedData64.has_value(),
		   "Heuristic Serialize data has no data?");
	std::string encoded = serializedData64.value();
	if (encoded.size() >= 2 && encoded.front() == '"' && encoded.back() == '"')
	{
		encoded = encoded.substr(1, encoded.size() - 2);
	}
	ByteReader br(encoded, true);
	heuristic->Deserialize(br);

	return heuristic;
}
HeuristicManifest::ClaimedBoundStruct
HeuristicManifest::Internal_PullClaimedBound(IBounds::BoundsID id)
{
	std::optional<std::string> claimedBound = InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string>
		{
			// JSON.GET key .Claimed.<ID>
			std::array<std::string, 3> get_cmd = {
				"JSON.GET",
				JSONDataTable,					// the Redis key
				std::format(".Claimed.{}", id)	// targetID is an int
			};

			return r.template command<std::optional<std::string>>(
				get_cmd.begin(), get_cmd.end());
		});
	ASSERT(claimedBound.has_value(), "ID not found!");
	Json json = Json::parse(claimedBound.value());
	ClaimedBoundStruct c;
	c.from_json(json);
	return c;
}

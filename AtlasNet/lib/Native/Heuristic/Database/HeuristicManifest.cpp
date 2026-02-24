#include "HeuristicManifest.hpp"

#include <sw/redis++/redis.h>

#include <boost/describe/enum_from_string.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include "Global/Serialize/ByteReader.hpp"
#include "Global/pch.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
std::optional<Json> ParseRedisJsonPayload(const std::optional<std::string>& payload)
{
	if (!payload || payload->empty() || *payload == "null")
	{
		return std::nullopt;
	}

	Json parsed = Json::parse(*payload, nullptr, false);
	if (parsed.is_discarded())
	{
		return std::nullopt;
	}
	if (parsed.is_array())
	{
		if (parsed.empty())
		{
			return std::nullopt;
		}
		parsed = parsed.front();
	}
	return parsed;
}
}  // namespace
static std::string_view StripQuotes(std::string_view str)
{
	if (!str.empty())
	{
		// Remove leading character if it is '[' or '"'
		if (str.front() == '"')
			str.remove_prefix(1);
		// Remove trailing character if it is ']' or '"'
		if (!str.empty() && str.back() == '"')
			str.remove_suffix(1);
	}
	return str;	 // make a new string to own the memory
}
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
	data_for_readers.clear();
	brs.clear();
	const auto manifestJson = ParseRedisJsonPayload(InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string> {
			return r.template command<std::optional<std::string>>("JSON.GET", JSONDataTable, ".");
		}));
	if (!manifestJson || !manifestJson->is_object())
	{
		return;
	}
	auto pendingIt = manifestJson->find(JSONPendingEntry);
	if (pendingIt == manifestJson->end() || !pendingIt->is_object())
	{
		return;
	}

	for (const auto& [_, pendingEntry] : pendingIt->items())
	{
		if (!pendingEntry.is_object())
		{
			continue;
		}
		PendingBoundStruct pendingBound;
		pendingBound.from_json(pendingEntry);
		data_for_readers.push_back(pendingBound.BoundsDataBase64);
		brs.emplace(pendingBound.ID, ByteReader(data_for_readers.back(), true));
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
		PendingBoundStruct p{.ID = ID, .BoundsDataBase64 = std::string(writer.as_string_base_64())};
		Internal_InsertPendingBound(p);
	}
}
long long HeuristicManifest::GetPendingBoundsCount() const
{
	const auto manifestJson = ParseRedisJsonPayload(InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string> {
			return r.template command<std::optional<std::string>>("JSON.GET", JSONDataTable, ".");
		}));
	if (!manifestJson || !manifestJson->is_object())
	{
		return 0;
	}
	auto pendingIt = manifestJson->find(JSONPendingEntry);
	if (pendingIt == manifestJson->end() || !pendingIt->is_object())
	{
		return 0;
	}
	return static_cast<long long>(pendingIt->size());
}
long long HeuristicManifest::GetClaimedBoundsCount() const
{
	const auto manifestJson = ParseRedisJsonPayload(InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string> {
			return r.template command<std::optional<std::string>>("JSON.GET", JSONDataTable, ".");
		}));
	if (!manifestJson || !manifestJson->is_object())
	{
		return 0;
	}
	auto claimedIt = manifestJson->find(JSONClaimedEntry);
	if (claimedIt == manifestJson->end() || !claimedIt->is_object())
	{
		return 0;
	}
	return static_cast<long long>(claimedIt->size());
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
			const auto result = r.template command<long long>("EVAL", kLuaScript, "2",
															  ClaimedHashTableNID2BoundData,
															  PendingHashTable, claim_key);
			return result != 0;
		});
}
void HeuristicManifest::Internal_SetActiveHeuristicType(IHeuristic::Type type)
{
	Internal_EnsureJsonTable();
	logger.DebugFormatted("Set HeuristicType {}", IHeuristic::TypeToString(type));
	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 4> set_type = {
				"JSON.SET", JSONDataTable,
				"." + JSONHeuristicTypeEntry,						   // ".HeuristicType"
				std::format("\"{}\"", IHeuristic::TypeToString(type))  // JSON string value
			};

			r.command(set_type.begin(), set_type.end());
		});
}
IHeuristic::Type HeuristicManifest::GetActiveHeuristicType() const
{
	std::optional<std::string> HeuristicTypeString = InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string>
		{
			// JSON.GET key .HeuristicData64
			std::array<std::string, 3> get_cmd = {
				"JSON.GET",
				JSONDataTable,	// the Redis key
				"." + JSONHeuristicTypeEntry
				// path to the field
			};

			return r.template command<std::optional<std::string>>(get_cmd.begin(), get_cmd.end());
		});

	if (!HeuristicTypeString.has_value())
		return IHeuristic::Type::eInvalid;
	// logger.DebugFormatted("GetActiveHeuristicType returned: [{}]", *HeuristicTypeString);
	const auto stripped_string = StripQuotes(*HeuristicTypeString);
	// logger.DebugFormatted("GetActiveHeuristicType stripped: [{}]", stripped_string);
	IHeuristic::Type t;
	IHeuristic::TypeFromString(stripped_string, t);
	return t;
}

std::optional<NetworkIdentity> HeuristicManifest::ShardFromPosition(const Transform& t)
{
	const auto heuristic = PullHeuristic();
	ASSERT(heuristic, "Pull heuristic returned nothing");
	const auto boundID = heuristic->QueryPosition(t.position);
	if (!boundID.has_value()) return std::nullopt;
	logger.DebugFormatted("found bound for position {}", *boundID);
	return ShardFromBoundID(*boundID);
	// bound->GetID()
}
std::optional<NetworkIdentity> HeuristicManifest::ShardFromBoundID(const IBounds::BoundsID id)
{
	const std::optional<ClaimedBoundStruct> claimedBound = Internal_PullClaimedBound(id);
	if (claimedBound.has_value())
	{
		return claimedBound->identity;
	}
	return std::nullopt;
}
void HeuristicManifest::GetClaimedBoundsAsByteReaders(
	std::vector<std::string>& data_for_readers,
	std::unordered_map<NetworkIdentity, std::pair<IBounds::BoundsID, ByteReader>>& brs)
{
	data_for_readers.clear();
	brs.clear();
	const auto manifestJson = ParseRedisJsonPayload(InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string> {
			return r.template command<std::optional<std::string>>("JSON.GET", JSONDataTable, ".");
		}));
	if (!manifestJson || !manifestJson->is_object())
	{
		return;
	}
	auto claimedIt = manifestJson->find(JSONClaimedEntry);
	if (claimedIt == manifestJson->end() || !claimedIt->is_object())
	{
		return;
	}

	for (const auto& [_, claimedEntry] : claimedIt->items())
	{
		if (!claimedEntry.is_object())
		{
			continue;
		}
		ClaimedBoundStruct claimedBound;
		claimedBound.from_json(claimedEntry);
		data_for_readers.push_back(claimedBound.BoundsDataBase64);
		brs.emplace(claimedBound.identity,
					std::make_pair(claimedBound.ID, ByteReader(data_for_readers.back(), true)));
	}
}
void HeuristicManifest::Internal_InsertPendingBound(const PendingBoundStruct& p)
{
	Json j;
	p.to_json(j);

	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 5> ensure_pending_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONPendingEntry,	 // .Pending
				"[]",					 // empty object if missing
				"NX"					 // only create if not exists
			};
			r.command(ensure_pending_cmd.begin(), ensure_pending_cmd.end());
			std::array<std::string, 3> check_id_cmd = {
				"JSON.GET", JSONDataTable,
				std::format("$.{}[?(@.ID == {})]", JSONPendingEntry, p.ID)};
			std::optional<std::string> existing = r.template command<std::optional<std::string>>(
				check_id_cmd.begin(), check_id_cmd.end());
			if (!existing.has_value() || existing->size() <= 2)	 // "[]" means empty
			{
				std::array<std::string, 4> append_cmd = {
					"JSON.ARRAPPEND", JSONDataTable,
					"." + JSONPendingEntry,	 // append to array
					j.dump()				 // JSON string representing PendingBoundStruct
				};
				r.command(append_cmd.begin(), append_cmd.end());
			}
			else
			{
				ASSERT(false, "This should never happen");
			}
		});
}
void HeuristicManifest::Internal_InsertClaimedBound(const ClaimedBoundStruct& c)
{
	Json j;
	c.to_json(j);

	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 5> ensure_claimed_cmd = {
				"JSON.SET", JSONDataTable,
				"." + JSONClaimedEntry,	 // e.g., .Claimed
				"[]",					 // empty object if missing
				"NX"					 // only create if not exists
			};
			r.command(ensure_claimed_cmd.begin(), ensure_claimed_cmd.end());

			std::array<std::string, 3> check_id_cmd = {
				"JSON.GET", JSONDataTable,
				std::format("$.{}[?(@.ID == {})]", JSONClaimedEntry, c.ID)};

			std::optional<std::string> existing = r.template command<std::optional<std::string>>(
				check_id_cmd.begin(), check_id_cmd.end());

			if (!existing.has_value() || existing->size() <= 2)	 // "[]" means empty
			{
				std::array<std::string, 4> set_object_cmd = {
					"JSON.ARRAPPEND", JSONDataTable,
					"." + JSONClaimedEntry,	 // e.g., .Claimed.0
					j.dump()				 // JSON string for this claimed entry
				};
				r.command(set_object_cmd.begin(), set_object_cmd.end());
			}
			else
			{
				ASSERT(false, "This should never happen");
			}
		});
}
void HeuristicManifest::PushHeuristic(const IHeuristic& h)
{
	Internal_SetActiveHeuristicType(h.GetType());
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

			std::array<std::string, 4> set_type = {
				"JSON.SET", JSONDataTable,
				"." + JSONHeuristicData64Entry,				   // ".HeuristicType"
				std::format("\"{}\"", bw.as_string_base_64())  // JSON string value
			};

			r.command(set_type.begin(), set_type.end());
		});
	logger.Debug("Heuristic Pushed");
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
	const auto serializedData64 = InternalDB::Get()->WithSync(
		[&](auto& r) -> std::optional<std::string>
		{
			// JSON.GET key .HeuristicData64
			std::array<std::string, 3> get_cmd = {
				"JSON.GET",
				JSONDataTable,	// the Redis key
				"." + JSONHeuristicData64Entry
				// path to the field
			};

			return r.template command<std::optional<std::string>>(get_cmd.begin(), get_cmd.end());
		});
	ASSERT(serializedData64.has_value(), "Heuristic Serialize data has no data?");
	std::string encoded = serializedData64.value();
	if (encoded.size() >= 2 && encoded.front() == '"' && encoded.back() == '"')
	{
		encoded = encoded.substr(1, encoded.size() - 2);
	}
	ByteReader br(encoded, true);
	heuristic->Deserialize(br);

	return heuristic;
}
std::optional<HeuristicManifest::ClaimedBoundStruct> HeuristicManifest::Internal_PullClaimedBound(
	IBounds::BoundsID id)
{
	const auto result = InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 3> get_id_cmd = {
				"JSON.GET", JSONDataTable, std::format("$.{}.[?(@.ID=={})]", JSONClaimedEntry, id)};
			std::optional<std::string> response = r.template command<std::optional<std::string>>(
				get_id_cmd.begin(), get_id_cmd.end());

			return response;
		});
	const Json json = result.has_value() ? Json::parse(*result) : Json();

	if (json.is_null() || !json.is_array() || (json.size() == 0) || json.front().is_null())
	{
		return std::nullopt;
	}
	ASSERT(json.is_array(), "Invalid response");
	ASSERT(json.size() <= 1, "This should never occur");
	ClaimedBoundStruct c;
	c.from_json(json.front());

	return c;
}
std::unique_ptr<IBounds> HeuristicManifest::ClaimNextPendingBound(const NetworkIdentity& claim_key)
{
	static const char* kLuaScript = R"lua(
-- KEYS[1] = JSON key
-- ARGV[1] = Pending array name
-- ARGV[2] = Claimed array name
-- ARGV[3] = Owner Base64
-- ARGV[4] = Owner Name

local key = KEYS[1]
local pending_field = ARGV[1]
local claimed_field = ARGV[2]
local owner_base64 = ARGV[3]
local owner_name = ARGV[4]

-- Ensure Claimed array exists
local claimed_type = redis.call("JSON.TYPE", key, claimed_field)
if claimed_type == false or claimed_type[1] == nil then
    redis.call("JSON.SET", key, claimed_field, "[]","NX")
end

-- Get length of Pending array
local pending_len = redis.call("JSON.ARRLEN", key, pending_field)
if pending_len == 0 then
    return nil
end

-- Pick the first entry in Pending
local pending_entry_json = redis.call("JSON.GET", key, pending_field .. "[0]")
local pending_entry = cjson.decode(pending_entry_json)
local id = pending_entry.ID

-- Append the entry to Claimed array
redis.call("JSON.ARRAPPEND", key, claimed_field, pending_entry_json)

-- Add Owner fields to the last element in Claimed array
local claimed_len = redis.call("JSON.ARRLEN", key, claimed_field)
local last_index = claimed_len - 1
redis.call("JSON.SET", key, claimed_field .. "[" .. last_index .. "].Owner64", "\"" .. owner_base64 .. "\"")
redis.call("JSON.SET", key, claimed_field .. "[" .. last_index .. "].OwnerName", "\"" .. owner_name .. "\"")

-- Remove the entry from Pending array
redis.call("JSON.ARRPOP", key, pending_field, 0)

-- Return the claimed ID as integer
return tonumber(id)
)lua";
	ByteWriter bw;
	claim_key.Serialize(bw);
	const auto claimedID = InternalDB::Get()->WithSync(
		[&](auto& r) -> auto
		{
			const auto result = r.template command<std::optional<long long>>(
				"EVAL", kLuaScript, "1", JSONDataTable, JSONPendingEntry, JSONClaimedEntry,
				bw.as_string_base_64(), claim_key.ToString());
			return result;
		});

	if (claimedID.has_value())
	{
		const auto BoundData = GetClaimedBound(*claimedID);
		ByteReader br(BoundData->BoundsDataBase64, true);
		auto bound = Internal_CreateIBoundInst();
		bound->Deserialize(br);
		return bound;
	}
	return nullptr;
}
std::unique_ptr<IBounds> HeuristicManifest::Internal_CreateIBoundInst()
{
	const auto hType = GetActiveHeuristicType();
	switch (hType)
	{
		case IHeuristic::Type::eGridCell:
			return std::make_unique<GridShape>();
			break;
		case IHeuristic::Type::eOctree:
		case IHeuristic::Type::eQuadtree:
		case IHeuristic::Type::eInvalid:
		case IHeuristic::Type::eNone:
			logger.ErrorFormatted("Unrecognized HeuristicType {}", IHeuristic::TypeToString(hType));
			ASSERT(false,
				   std::format("Unrecognized HeuristicType {}", IHeuristic::TypeToString(hType))
					   .c_str());

			return nullptr;
			break;
	};
}
std::optional<HeuristicManifest::ClaimedBoundStruct> HeuristicManifest::GetClaimedBound(
	IBounds::BoundsID id)
{
	std::optional<std::string> HeuristicTypeString = InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			// JSON.GET key $.Claimed[?(@.ID == <id>)]
			std::array<std::string, 3> get_cmd = {
				"JSON.GET",
				JSONDataTable,	// the Redis key
				std::format("$.{}[?(@.ID == {})]", JSONClaimedEntry, id)};

			return r.template command<std::optional<std::string>>(get_cmd.begin(), get_cmd.end());
		});

	if (!HeuristicTypeString)
		return std::nullopt;

	ClaimedBoundStruct c;
	Json json = Json::parse(HeuristicTypeString.value());
	// logger.DebugFormatted("GetClaimedBound returned {}", json.dump(4));
	c.from_json(json.front());
	return c;
}
void HeuristicManifest::Internal_EnsureJsonTable()
{
	InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			// JSON.GET key .HeuristicData64
			std::array<std::string, 5> ensure_claimed_cmd = {
				"JSON.SET", JSONDataTable,
				".",   // e.g., .Claimed
				"{}",  // empty object if missing
				"NX"   // only create if not exists
			};
			return r.command(ensure_claimed_cmd.begin(), ensure_claimed_cmd.end());
		});
}

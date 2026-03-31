#pragma once

#include <sw/redis++/redis.h>

#include <array>
#include <boost/container/flat_map.hpp>
#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <charconv>
#include <concepts>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"

class HeuristicManifest : public Singleton<HeuristicManifest>
{
	Log logger = Log("HeuristicManifest");

	const std::string HeuristicNamespace = "Heuristic:";

	const std::string HeuristicTypeKey = HeuristicNamespace + "Type";
	const std::string HeuristicVersionKey = HeuristicNamespace + "Version";
	const std::string HeuristicSerializeDataKey = HeuristicNamespace + "Data";

	const std::string OwnershipNamespace = HeuristicNamespace + ":Ownership:";

	const std::string OwnershipTableVersion = OwnershipNamespace + "Version";
	const std::string PendingIDsSet = OwnershipNamespace + "PendingIDs";
	// const std::string BoundID2NIDMap =
	//	OwnershipNamespace + "BoundID -> NetID Map";  //[BoundID,NetworkIdentity]
	const std::string NID2BoundIDMap =
		OwnershipNamespace + "NetID -> BoundID Map";  //[BoundID,NetworkIdentity]

	struct HeuristicLocalCache
	{
		IHeuristic::Type ActiveHeuristic = IHeuristic::Type::eNone;
		std::unique_ptr<IHeuristic> CachedHeuristic;
		std::atomic<long long> HeuristicVersion = 0;
	} localHeuristicCache;
	std::shared_mutex HeuristicFetchMutex;

	struct OwnershipLocalCache
	{
		std::atomic<long long> OwnershipTableVersion = 0;
		boost::container::flat_map<BoundsID, UUID> BoundID2Shard;
		boost::container::flat_map<UUID, BoundsID> Shard2BoundID;
		uint32_t PendingBoundCount, ClaimedBountCount;
	} localownershipCache;
	std::shared_mutex OwnershitFetchMutex;

   public:
	class OwnershipStateWrapper
	{
		friend HeuristicManifest;
		const OwnershipLocalCache& cache;

	   protected:
		OwnershipStateWrapper(OwnershipLocalCache& cache) : cache(cache) {}

	   public:
		std::optional<NetworkIdentity> GetBoundOwner(BoundsID bID) const
		{
			if (auto itf = cache.BoundID2Shard.find(bID); itf != cache.BoundID2Shard.end())
			{
				return NetworkIdentity::MakeIDShard(itf->second);
			}
			return std::nullopt;
		}
		std::optional<BoundsID> GetBoundOwner(const NetworkIdentity& NID) const
		{
			if (auto itf = cache.Shard2BoundID.find(NID.ID); itf != cache.Shard2BoundID.end())
			{
				return itf->second;
			}
			return std::nullopt;
		}
	};

	// std::optional<BoundsID> BoundIDFromShard(const NetworkIdentity& id);

	[[nodiscard]] std::optional<BoundsID> ClaimNextPendingBound(const NetworkIdentity& claim_key);

	//[[nodiscard]] long long GetPendingBoundsCount() const;

	//[[nodiscard]] long long GetClaimedBoundsCount() const;

	bool ReleaseClaimedBound(const NetworkIdentity& NetID);

	template <typename FN>
		requires std::is_invocable_v<FN, const IHeuristic&>
	auto PullHeuristic(FN&& f);
	template <typename FN>
		requires std::is_invocable_v<FN, const IBounds&>
	auto PullHeuristicBound(BoundsID ID, FN&& f)
	{
		return PullHeuristic([&](const IHeuristic& h) { return f(h.GetBound(ID)); });
	}
	void PushHeuristic(const IHeuristic& h);
	template <typename FN>
		requires std::is_invocable_v<FN, const OwnershipStateWrapper&>
	auto QueryOwnershipState(FN&& f)
	{
		const std::optional<std::string> version_s = InternalDB::Get()->Get(OwnershipTableVersion);
		if (!version_s)
		{
			throw std::runtime_error("Heuristic timestamp missing");
		}
		const long long versionStamp = std::stoll(version_s.value());
		if (versionStamp != localownershipCache.OwnershipTableVersion)
		{
			std::unique_lock lock_unique(OwnershitFetchMutex);
			if (versionStamp != localownershipCache.OwnershipTableVersion)	// RECHECK WITH LOCK
			{
				logger.DebugFormatted("QueryOwnershipState Cache out of date. Fetching version {}",
									  versionStamp);
				std::unordered_map<std::string, std::string> values =
					InternalDB::Get()->HGetAll(NID2BoundIDMap);
				localownershipCache.ClaimedBountCount = 0;
				localownershipCache.PendingBoundCount = InternalDB::Get()->SCard(PendingIDsSet);
				localownershipCache.BoundID2Shard.clear();
				localownershipCache.Shard2BoundID.clear();
				for (const auto& [ShardID_s, BoundID_s] : values)
				{
					UUID shardID = UUIDGen::FromString(ShardID_s);
					BoundsID boundID;
					std::from_chars(BoundID_s.data(), BoundID_s.data() + BoundID_s.size(), boundID);

					localownershipCache.ClaimedBountCount++;
					localownershipCache.BoundID2Shard.insert(std::make_pair(boundID, shardID));
					localownershipCache.Shard2BoundID.insert(std::make_pair(shardID, boundID));
				}
				localownershipCache.OwnershipTableVersion = versionStamp;
				logger.DebugFormatted("QueryOwnershipState Cache Fetch Done");
			}
		}

		std::shared_lock lock_shared(OwnershitFetchMutex);

		OwnershipStateWrapper cacheWrapper(localownershipCache);
		return f(cacheWrapper);
	}
	//[[nodiscard]] std::optional<NetworkIdentity> ShardFromPosition(const Transform& t);

	//[[nodiscard]] std::optional<NetworkIdentity> ShardFromBoundID(const BoundsID id);
	// void StorePendingBound(const IBounds& bound);

   private:
	void Internal_SetActiveHeuristicType(IHeuristic::Type type);
	void Internal_ParseHeuristic(IHeuristic::Type type, std::string_view data);
	void Internal_OwnershipTableIncreaseVersion()
	{
		InternalDB::Get()->WithSync([this](auto& r) { r.incr(OwnershipTableVersion); });
	}
};

template <typename FN>
	requires std::is_invocable_v<FN, const IHeuristic&>
inline auto HeuristicManifest::PullHeuristic(FN&& f)
{
	while (true)
	{
		const std::optional<std::string> timestamp_s = InternalDB::Get()->Get(HeuristicVersionKey);
		if (!timestamp_s)
		{
			throw std::runtime_error("Heuristic timestamp missing");
		}
		const long long PushTimeStamp = std::stoll(timestamp_s.value());

		if (PushTimeStamp != localHeuristicCache.HeuristicVersion)
		{
			auto heuData = InternalDB::Get()->Get(HeuristicSerializeDataKey);
			auto heuType_s = InternalDB::Get()->Get(HeuristicTypeKey);
			if (!heuData.has_value() || !heuType_s.has_value())
			{
				logger.ErrorFormatted("Failed to pull heuristic, trying again");
				continue;
			}
			std::unique_lock lock_unique(HeuristicFetchMutex);

			if (PushTimeStamp != localHeuristicCache.HeuristicVersion)
			{
				logger.DebugFormatted("Heuristic Cache out of date. Fetching version {}",
									  PushTimeStamp);
				IHeuristic::Type hType;
				IHeuristic::TypeFromString(*heuType_s, hType);

				Internal_ParseHeuristic(hType, *heuData);
				localHeuristicCache.HeuristicVersion = PushTimeStamp;

				logger.DebugFormatted("Heuristic Cache Fetch Done");
			}
		}
		break;
	}
	std::shared_lock lock_shared(HeuristicFetchMutex);
	return f(*localHeuristicCache.CachedHeuristic);
}
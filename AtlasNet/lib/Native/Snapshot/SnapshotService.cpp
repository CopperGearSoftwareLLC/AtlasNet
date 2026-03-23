#include "SnapshotService.hpp"

#include <charconv>
#include <chrono>
#include <execution>
#include <string>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/IBounds.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Network/NetworkCredentials.hpp"

namespace
{
constexpr std::string_view kEntitySnapshotBoundsIndexHashTable = "Entity:Snapshot:Bounds";
constexpr std::string_view kHeuristicVersionKey = "Heuristic:Version";

std::string SerializeBoundID(BoundsID boundID)
{
	return std::to_string(boundID);
}

std::string SerializeEntityID(const AtlasEntityID& entityID)
{
	return UUIDGen::ToString(entityID);
}

std::string EntityEntrySnapshotHashKey(BoundsID boundID)
{
	return "Entity:Snapshot:Bound:" + SerializeBoundID(boundID) + ":Entities";
}

std::unordered_map<std::string, std::string> FetchPerEntitySnapshotEntries(BoundsID boundID)
{
	return InternalDB::Get()->HGetAll(EntityEntrySnapshotHashKey(boundID));
}

void UpsertPerEntitySnapshotPayload(BoundsID boundID, const std::string& field,
									std::string_view payload)
{
	(void)InternalDB::Get()->HSet(EntityEntrySnapshotHashKey(boundID), field, payload);
}

void DeletePerEntitySnapshotPayload(BoundsID boundID, const std::string& field)
{
	(void)InternalDB::Get()->HDel(EntityEntrySnapshotHashKey(boundID), {field});
}

std::optional<long long> ParseInt64(const std::optional<std::string>& rawValue)
{
	if (!rawValue.has_value())
	{
		return std::nullopt;
	}

	long long parsedValue = 0;
	const auto [ptr, ec] = std::from_chars(rawValue->data(), rawValue->data() + rawValue->size(),
										   parsedValue);
	if (ec != std::errc() || ptr != rawValue->data() + rawValue->size())
	{
		return std::nullopt;
	}
	return parsedValue;
}
}  // namespace

void SnapshotService::SnapshotThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_SNAPSHOT_INTERNAL_MS);
	while (!st.stop_requested())
	{
		RecoverClaimedBoundSnapshotIfNeeded();
		RecoverOrphanedEntitiesForCurrentHeuristicIfNeeded();
		UploadSnapshot();

		if (st.stop_requested())
		{
			break;
		}

		// Align roughly to system clock
		auto now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

		// Compute time until next "tick" of interval
		auto next_tick_ms = ((now_ms.count() / interval.count()) + 1) * interval.count();
		auto sleep_duration = milliseconds(next_tick_ms - now_ms.count());

		// Add tiny random jitter to avoid perfect collisions
		sleep_duration += milliseconds(rand() % 10);  // 0-9 ms jitter

		// Sleep until next tick
		if (sleep_duration > milliseconds(0))
			std::this_thread::sleep_for(sleep_duration);

		if (st.stop_requested())
			break;
	}
}
SnapshotService::SnapshotService()
{
	snapshotThread = std::jthread([this](std::stop_token st) { SnapshotThreadLoop(st); });
};

void SnapshotService::FlushClaimedBoundSnapshot()
{
	std::lock_guard lock(recoveryMutex);
	UploadSnapshot();
}

void SnapshotService::TouchBoundSnapshotIndex(BoundsID boundID)
{
	(void)InternalDB::Get()->HSet(kEntitySnapshotBoundsIndexHashTable, SerializeBoundID(boundID),
								  std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
													 std::chrono::system_clock::now().time_since_epoch())
													 .count()));
}

void SnapshotService::RemoveBoundSnapshotIndex(BoundsID boundID)
{
	(void)InternalDB::Get()->HDel(kEntitySnapshotBoundsIndexHashTable, {SerializeBoundID(boundID)});
}

void SnapshotService::UpsertBoundEntitySnapshot(BoundsID boundID, const AtlasEntity& entity)
{
	TouchBoundSnapshotIndex(boundID);

	ByteWriter entityWriter;
	entity.Serialize(entityWriter);
	UpsertPerEntitySnapshotPayload(boundID, SerializeEntityID(entity.Entity_ID),
								   entityWriter.as_string_view());
}

void SnapshotService::UpsertClaimedBoundEntitySnapshot(const AtlasEntity& entity)
{
	if (!BoundLeaser::Get().HasBound())
	{
		return;
	}

	UpsertBoundEntitySnapshot(BoundLeaser::Get().GetBoundID(), entity);
}

void SnapshotService::DeleteBoundEntitySnapshot(BoundsID boundID, const AtlasEntityID& entityID)
{
	const std::string entityField = SerializeEntityID(entityID);
	DeletePerEntitySnapshotPayload(boundID, entityField);

	if (InternalDB::Get()->HLen(EntityEntrySnapshotHashKey(boundID)) == 0)
	{
		(void)InternalDB::Get()->DelKey(EntityEntrySnapshotHashKey(boundID));
		RemoveBoundSnapshotIndex(boundID);
	}
}

void SnapshotService::DeleteClaimedBoundEntitySnapshot(const AtlasEntityID& entityID)
{
	if (!BoundLeaser::Get().HasBound())
	{
		return;
	}

	DeleteBoundEntitySnapshot(BoundLeaser::Get().GetBoundID(), entityID);
}

void SnapshotService::RecoverClaimedBoundSnapshotIfNeeded()
{
	std::lock_guard lock(recoveryMutex);
	if (!BoundLeaser::Get().HasBound())
	{
		recoveredBoundID.reset();
		recoveredHeuristicVersion.reset();
		return;
	}

	const BoundsID boundID = BoundLeaser::Get().GetBoundID();
	if (recoveredBoundID.has_value() && recoveredBoundID.value() == boundID)
	{
		return;
	}

	recoveredBoundID = boundID;
	recoveredHeuristicVersion.reset();
	RecoverBoundSnapshot(boundID);
	ReconcileClaimedBoundEntityRecords();
}

bool SnapshotService::RecoverBoundSnapshot(BoundsID boundID)
{
	const auto perEntityPayloads = FetchPerEntitySnapshotEntries(boundID);
	if (!perEntityPayloads.empty())
	{
		size_t recoveredCount = 0;
		size_t skippedLiveOwnedElsewhereCount = 0;
		size_t recoveredFromDeadOwnerCount = 0;
		const ShardID selfShardID = NetworkCredentials::Get().GetID().ID;
		std::vector<std::string> livePeerKeys;
		HealthManifest::Get().GetLivePings(livePeerKeys);
		std::unordered_set<NetworkIdentity> livePeerSet;
		livePeerSet.reserve(livePeerKeys.size());
		for (const std::string& livePeerKey : livePeerKeys)
		{
			ByteReader livePeerReader(livePeerKey);
			NetworkIdentity livePeerIdentity;
			livePeerIdentity.Deserialize(livePeerReader);
			livePeerSet.insert(std::move(livePeerIdentity));
		}
		for (const auto& [entityIDStr, payload] : perEntityPayloads)
		{
			(void)entityIDStr;
			ByteReader reader(payload);
			AtlasEntity entity;
			entity.Deserialize(reader);
			const std::optional<ShardID> ownerShard =
				GlobalEntityLedger::Get().GetEntityOwnerShard(entity.Entity_ID);
			if (ownerShard.has_value() && ownerShard.value() != selfShardID)
			{
				const NetworkIdentity ownerIdentity = NetworkIdentity::MakeIDShard(*ownerShard);
				if (livePeerSet.contains(ownerIdentity))
				{
					++skippedLiveOwnedElsewhereCount;
					continue;
				}
				++recoveredFromDeadOwnerCount;
			}
			if (EntityLedger::Get().ExistsEntity(entity.Entity_ID))
			{
				continue;
			}

			EntityLedger::Get().AddEntity(entity);
			++recoveredCount;
		}

			if (recoveredCount > 0 || skippedLiveOwnedElsewhereCount > 0 ||
				recoveredFromDeadOwnerCount > 0)
			{
				logger.WarningFormatted(
					"Recovered {} entities for claimed bound {} from per-entity database snapshot "
					"(skipped {} live-owned elsewhere, reclaimed {} from dead owners)",
					recoveredCount, boundID, skippedLiveOwnedElsewhereCount,
					recoveredFromDeadOwnerCount);
			}
		return recoveredCount > 0;
	}

	logger.DebugFormatted("No entity snapshot found for bound {}", boundID);
	return false;
}

void SnapshotService::RecoverOrphanedEntitiesForCurrentHeuristicIfNeeded()
{
	if (!BoundLeaser::Get().HasBound())
	{
		recoveredHeuristicVersion.reset();
		return;
	}

	const std::optional<long long> heuristicVersion =
		ParseInt64(InternalDB::Get()->Get(std::string(kHeuristicVersionKey)));
	if (!heuristicVersion.has_value())
	{
		return;
	}
	if (recoveredHeuristicVersion.has_value() &&
		recoveredHeuristicVersion.value() == heuristicVersion.value())
	{
		return;
	}

	const BoundsID claimedBoundID = BoundLeaser::Get().GetBoundID();
	const ShardID selfShardID = NetworkCredentials::Get().GetID().ID;
	std::vector<std::string> livePeerKeys;
	HealthManifest::Get().GetLivePings(livePeerKeys);
	std::unordered_set<NetworkIdentity> livePeerSet;
	livePeerSet.reserve(livePeerKeys.size());
	for (const std::string& livePeerKey : livePeerKeys)
	{
		ByteReader livePeerReader(livePeerKey);
		NetworkIdentity livePeerIdentity;
		livePeerIdentity.Deserialize(livePeerReader);
		livePeerSet.insert(std::move(livePeerIdentity));
	}

	size_t recoveredCount = 0;
	size_t movedSnapshotCount = 0;
	const std::vector<std::string> indexedBounds =
		InternalDB::Get()->HKeys(kEntitySnapshotBoundsIndexHashTable);

	BoundLeaser::Get().GetBound(
		[&](const IBounds& currentBound)
		{
			for (const std::string& boundIDStr : indexedBounds)
			{
				BoundsID snapshotBoundID = 0;
				const auto [ptr, ec] = std::from_chars(boundIDStr.data(),
												   boundIDStr.data() + boundIDStr.size(),
												   snapshotBoundID);
				if (ec != std::errc() || ptr != boundIDStr.data() + boundIDStr.size())
				{
					continue;
				}

				const auto perEntityPayloads = FetchPerEntitySnapshotEntries(snapshotBoundID);
				for (const auto& [entityIDStr, payload] : perEntityPayloads)
				{
					(void)entityIDStr;
					ByteReader reader(payload);
					AtlasEntity entity;
					entity.Deserialize(reader);

					if (EntityLedger::Get().ExistsEntity(entity.Entity_ID) ||
						!currentBound.Contains(entity.transform.position))
					{
						continue;
					}

					const std::optional<ShardID> ownerShard =
						GlobalEntityLedger::Get().GetEntityOwnerShard(entity.Entity_ID);
					if (ownerShard.has_value() && ownerShard.value() != selfShardID)
					{
						const NetworkIdentity ownerIdentity =
							NetworkIdentity::MakeIDShard(ownerShard.value());
						if (livePeerSet.contains(ownerIdentity))
						{
							continue;
						}
					}

					EntityLedger::Get().AddEntity(entity);
					++recoveredCount;

					if (snapshotBoundID != claimedBoundID)
					{
						DeleteBoundEntitySnapshot(snapshotBoundID, entity.Entity_ID);
						++movedSnapshotCount;
					}
				}
			}
		});

	recoveredHeuristicVersion = heuristicVersion;
	if (recoveredCount > 0 || movedSnapshotCount > 0)
	{
		logger.WarningFormatted(
			"Recovered {} orphaned entities into claimed bound {} after heuristic version {} "
			"(moved {} stale snapshot entries)",
			recoveredCount, claimedBoundID, heuristicVersion.value(), movedSnapshotCount);
		ReconcileClaimedBoundEntityRecords();
	}
}

void SnapshotService::ReconcileClaimedBoundEntityRecords()
{
	if (!BoundLeaser::Get().HasBound())
	{
		return;
	}

	std::vector<AtlasEntityID> entityIDs;
	entityIDs.reserve(EntityLedger::Get().GetEntityCount());
	EntityLedger::Get().ForEachEntityRead(
		std::execution::seq,
		[&](const AtlasEntity& entity) { entityIDs.push_back(entity.Entity_ID); });

	const ShardID selfShardID = NetworkCredentials::Get().GetID().ID;
	GlobalEntityLedger::Get().ReplaceShardEntityRecords(selfShardID, entityIDs);
	logger.DebugFormatted("Reconciled {} entity ownership records for shard {}",
						  entityIDs.size(), UUIDGen::ToString(selfShardID));
}

void SnapshotService::UploadSnapshot()
{
	if (BoundLeaser::Get().HasBound())
	{
		BoundsID boundID = BoundLeaser::Get().GetBoundID();
		if (EntityLedger::Get().GetEntityCount() > 0)
		{
			TouchBoundSnapshotIndex(boundID);
			EntityLedger::Get().ForEachEntityRead(std::execution::seq,
												  [&](const AtlasEntity& entity)
												  {
													  ByteWriter perEntityWriter;
													  entity.Serialize(perEntityWriter);
													  UpsertPerEntitySnapshotPayload(
														  boundID, SerializeEntityID(entity.Entity_ID),
														  perEntityWriter.as_string_view());
												  });
		}
		else
		{
			(void)InternalDB::Get()->DelKey(EntityEntrySnapshotHashKey(boundID));
			RemoveBoundSnapshotIndex(boundID);
		}
		ReconcileClaimedBoundEntityRecords();
	}
}
void SnapshotService::FetchEntityListSnapshot(
	std::unordered_map<BoundsID, std::vector<AtlasEntity>>& data)
{
	data.clear();
	const std::vector<std::string> indexedBounds =
		InternalDB::Get()->HKeys(kEntitySnapshotBoundsIndexHashTable);
	data.reserve(indexedBounds.size());
	for (const std::string& boundIDStr : indexedBounds)
	{
		const BoundsID boundID = std::stoi(boundIDStr);
		data.emplace(boundID, std::vector<AtlasEntity>{});
		auto& vec = data.at(boundID);
		const auto entries = FetchPerEntitySnapshotEntries(boundID);
		for (const auto& [entityIDStr, payload] : entries)
		{
			(void)entityIDStr;
			ByteReader reader(payload);
			vec.emplace_back(AtlasEntity{}).Deserialize(reader);
		}
	}
}
void SnapshotService::FetchBoundsTransformList(
	std::unordered_map<BoundsID, std::vector<AtlasTransform>>& transforms)
{
	transforms.clear();
	const std::vector<std::string> indexedBounds =
		InternalDB::Get()->HKeys(kEntitySnapshotBoundsIndexHashTable);
	transforms.reserve(indexedBounds.size());
	for (const std::string& boundIDStr : indexedBounds)
	{
		const BoundsID boundID = std::stoi(boundIDStr);
		transforms.emplace(boundID, std::vector<AtlasTransform>{});
		auto& vec = transforms.at(boundID);
		const auto entries = FetchPerEntitySnapshotEntries(boundID);
		for (const auto& [entityIDStr, payload] : entries)
		{
			(void)entityIDStr;
			ByteReader reader(payload);
			AtlasEntity entity;
			entity.Deserialize(reader);
			vec.push_back(entity.transform);
		}
	}
}
void SnapshotService::FetchAllTransforms(std::vector<AtlasTransform>& transforms)
{
	transforms.clear();
	const std::vector<std::string> indexedBounds =
		InternalDB::Get()->HKeys(kEntitySnapshotBoundsIndexHashTable);
	for (const std::string& boundIDStr : indexedBounds)
	{
		const BoundsID boundID = std::stoi(boundIDStr);
		const auto entries = FetchPerEntitySnapshotEntries(boundID);
		for (const auto& [entityIDStr, payload] : entries)
		{
			(void)entityIDStr;
			ByteReader reader(payload);
			AtlasEntity entity;
			entity.Deserialize(reader);
			transforms.push_back(entity.transform);
		}
	}
}

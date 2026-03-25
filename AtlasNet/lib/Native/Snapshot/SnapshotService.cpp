#include "SnapshotService.hpp"

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <execution>
#include <string>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/IBounds.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Interlink.hpp"
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

std::optional<BoundsID> QueryExpectedSnapshotBound(const AtlasEntity& entity)
{
	return HeuristicManifest::Get().PullHeuristic(
		[&](const IHeuristic& heuristic) { return heuristic.QueryPosition(entity.transform.position); });
}

struct LiveShardEntityPresence
{
	std::unordered_set<AtlasEntityID> entityIDs;
	std::unordered_set<std::string> responsiveShardIDs;
};

LiveShardEntityPresence CollectLiveShardEntityPresence()
{
	LiveShardEntityPresence presence;
	const NetworkIdentity selfID = NetworkCredentials::Get().GetID();
	presence.responsiveShardIDs.insert(UUIDGen::ToString(selfID.ID));
	EntityLedger::Get().ForEachEntityRead(
		std::execution::seq,
		[&](const AtlasEntity& entity) { presence.entityIDs.insert(entity.Entity_ID); });

	std::vector<std::string> livePeerKeys;
	HealthManifest::Get().GetLivePings(livePeerKeys);

	std::vector<NetworkIdentity> liveShardPeers;
	liveShardPeers.reserve(livePeerKeys.size());
	for (const std::string& livePeerKey : livePeerKeys)
	{
		ByteReader livePeerReader(livePeerKey);
		NetworkIdentity livePeerIdentity;
		livePeerIdentity.Deserialize(livePeerReader);
		if (livePeerIdentity.Type == NetworkIdentityType::eShard && livePeerIdentity != selfID)
		{
			liveShardPeers.push_back(std::move(livePeerIdentity));
		}
	}

	if (liveShardPeers.empty())
	{
		return presence;
	}

	std::mutex presenceMutex;
	std::condition_variable presenceCv;
	std::unordered_set<std::string> pendingShardIDs;
	pendingShardIDs.reserve(liveShardPeers.size());
	for (const NetworkIdentity& peer : liveShardPeers)
	{
		pendingShardIDs.insert(UUIDGen::ToString(peer.ID));
	}

	const auto subscription =
		Interlink::Get().GetPacketManager().Subscribe<LocalEntityListRequestPacket>(
			[&](const LocalEntityListRequestPacket& packet, const PacketManager::PacketInfo& info)
			{
				if (packet.status != LocalEntityListRequestPacket::MsgStatus::eResponse ||
					info.sender.Type != NetworkIdentityType::eShard ||
					!std::holds_alternative<std::vector<AtlasEntityMinimal>>(packet.Response_Entities))
				{
					return;
				}

				std::lock_guard lock(presenceMutex);
				presence.responsiveShardIDs.insert(UUIDGen::ToString(info.sender.ID));
				pendingShardIDs.erase(UUIDGen::ToString(info.sender.ID));
				for (const AtlasEntityMinimal& entity :
					 std::get<std::vector<AtlasEntityMinimal>>(packet.Response_Entities))
				{
					presence.entityIDs.insert(entity.Entity_ID);
				}
				presenceCv.notify_all();
			});

	LocalEntityListRequestPacket queryPacket;
	queryPacket.status = LocalEntityListRequestPacket::MsgStatus::eQuery;
	queryPacket.Request_IncludeMetadata = false;
	for (const NetworkIdentity& peer : liveShardPeers)
	{
		Interlink::Get().SendMessage(peer, queryPacket, NetworkMessageSendFlag::eReliableNow);
	}

	std::unique_lock lock(presenceMutex);
	(void)presenceCv.wait_for(
		lock, std::chrono::milliseconds(250), [&]() { return pendingShardIDs.empty(); });
	return presence;
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
		RecoverMissedEntitiesForClaimedBound();
		ReconcileClaimedBoundEntityRecords();
		return;
	}

	recoveredBoundID = boundID;
	recoveredHeuristicVersion.reset();
	RecoverBoundSnapshot(boundID);
	RecoverMissedEntitiesForClaimedBound();
	ReconcileClaimedBoundEntityRecords();
}

bool SnapshotService::RecoverBoundSnapshot(BoundsID boundID)
{
	const auto perEntityPayloads = FetchPerEntitySnapshotEntries(boundID);
	if (!perEntityPayloads.empty())
	{
		size_t recoveredCount = 0;
		size_t skippedCopiedSnapshotCount = 0;
		size_t skippedMissingOwnerCount = 0;
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
			if (!ownerShard.has_value())
			{
				++skippedMissingOwnerCount;
				continue;
			}

			const std::optional<BoundsID> expectedBoundID = QueryExpectedSnapshotBound(entity);
			if (expectedBoundID.has_value() && expectedBoundID.value() != boundID &&
				ownerShard.value() != selfShardID)
			{
				// This looks like a copied snapshot row. Keep the payload intact so the
				// authoritative owner/discrepancy recovery path can reclaim it later if needed.
				++skippedCopiedSnapshotCount;
				continue;
			}

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

			if (recoveredCount > 0 || skippedCopiedSnapshotCount > 0 ||
				skippedMissingOwnerCount > 0 ||
				skippedLiveOwnedElsewhereCount > 0 || recoveredFromDeadOwnerCount > 0)
			{
				logger.WarningFormatted(
					"Recovered {} entities for claimed bound {} from per-entity database snapshot "
					"(skipped {} copied snapshots for later authoritative recovery, skipped {} without owner records, skipped {} "
					"live-owned elsewhere, reclaimed {} from dead owners)",
					recoveredCount, boundID, skippedCopiedSnapshotCount,
					skippedMissingOwnerCount,
					skippedLiveOwnedElsewhereCount,
					recoveredFromDeadOwnerCount);
			}
		return recoveredCount > 0;
	}

	logger.DebugFormatted("No entity snapshot found for bound {}", boundID);
	return false;
}

void SnapshotService::RecoverMissedEntitiesForClaimedBound()
{
	if (!BoundLeaser::Get().HasBound())
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

	std::unordered_map<AtlasEntityID, EntitySnapshotRecord> snapshotsByEntityID;
	FetchEntitySnapshotsByID(snapshotsByEntityID);
	std::unordered_map<AtlasEntityID, ShardID> entityOwners;
	GlobalEntityLedger::Get().GetAllEntityOwners(entityOwners);
	const LiveShardEntityPresence liveShardPresence = CollectLiveShardEntityPresence();

	size_t recoveredOwnedBySelfCount = 0;
	size_t reclaimedFromDeadOwnerCount = 0;
	size_t reclaimedMissingFromLiveOwnerCount = 0;
	size_t skippedMissingSnapshotCount = 0;
	size_t skippedUnresponsiveLiveOwnerCount = 0;
	size_t movedSnapshotCount = 0;

	BoundLeaser::Get().GetBound(
		[&](const IBounds& currentBound)
		{
			for (const auto& [entityID, ownerShard] : entityOwners)
			{
				if (liveShardPresence.entityIDs.contains(entityID))
				{
					continue;
				}

				const auto snapshotIt = snapshotsByEntityID.find(entityID);
				if (snapshotIt == snapshotsByEntityID.end())
				{
					++skippedMissingSnapshotCount;
					continue;
				}

				const EntitySnapshotRecord& snapshotRecord = snapshotIt->second;
				const AtlasEntity& entity = snapshotRecord.entity;
				if (!currentBound.Contains(entity.transform.position))
				{
					continue;
				}

				if (ownerShard == selfShardID)
				{
					++recoveredOwnedBySelfCount;
				}
				else
				{
					const NetworkIdentity ownerIdentity = NetworkIdentity::MakeIDShard(ownerShard);
					if (livePeerSet.contains(ownerIdentity))
					{
						if (!liveShardPresence.responsiveShardIDs.contains(
								UUIDGen::ToString(ownerShard)))
						{
							++skippedUnresponsiveLiveOwnerCount;
							continue;
						}

						++reclaimedMissingFromLiveOwnerCount;
					}
					else
					{
						++reclaimedFromDeadOwnerCount;
					}

					GlobalEntityLedger::Get().DeclareEntityRecord(selfShardID, entityID);
				}

				EntityLedger::Get().AddEntity(entity);
				if (snapshotRecord.boundID != claimedBoundID)
				{
					DeleteBoundEntitySnapshot(snapshotRecord.boundID, entityID);
					++movedSnapshotCount;
				}
			}
		});

		if (recoveredOwnedBySelfCount > 0 || reclaimedFromDeadOwnerCount > 0 ||
			reclaimedMissingFromLiveOwnerCount > 0 || skippedMissingSnapshotCount > 0 ||
			skippedUnresponsiveLiveOwnerCount > 0 || movedSnapshotCount > 0)
	{
		logger.WarningFormatted(
			"Backup-recovered entities for claimed bound {} using Entity:EntityOwner discrepancies "
			"(recovered {} already-owned, reclaimed {} from dead owners, reclaimed {} missing "
			"from responsive live owners, skipped {} without snapshots, skipped {} behind "
			"unresponsive live owners, moved {} stale snapshot entries)",
			claimedBoundID, recoveredOwnedBySelfCount, reclaimedFromDeadOwnerCount,
			reclaimedMissingFromLiveOwnerCount, skippedMissingSnapshotCount,
			skippedUnresponsiveLiveOwnerCount, movedSnapshotCount);
	}
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
	std::unordered_map<AtlasEntityID, SnapshotService::EntitySnapshotRecord> snapshotsByEntityID;
	FetchEntitySnapshotsByID(snapshotsByEntityID);
	std::unordered_map<AtlasEntityID, ShardID> entityOwners;
	GlobalEntityLedger::Get().GetAllEntityOwners(entityOwners);

	BoundLeaser::Get().GetBound(
		[&](const IBounds& currentBound)
		{
			for (const auto& [entityID, ownerShard] : entityOwners)
			{
				if (ownerShard == selfShardID)
				{
					continue;
				}

				const NetworkIdentity ownerIdentity = NetworkIdentity::MakeIDShard(ownerShard);
				if (livePeerSet.contains(ownerIdentity))
				{
					continue;
				}

				const auto snapshotIt = snapshotsByEntityID.find(entityID);
				if (snapshotIt == snapshotsByEntityID.end())
				{
					continue;
				}

				const AtlasEntity& entity = snapshotIt->second.entity;
				if (EntityLedger::Get().ExistsEntity(entity.Entity_ID) ||
					!currentBound.Contains(entity.transform.position))
				{
					continue;
				}

				EntityLedger::Get().AddEntity(entity);
				++recoveredCount;

				if (snapshotIt->second.boundID != claimedBoundID)
				{
					DeleteBoundEntitySnapshot(snapshotIt->second.boundID, entity.Entity_ID);
					++movedSnapshotCount;
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
void SnapshotService::FetchEntitySnapshotsByID(
	std::unordered_map<AtlasEntityID, EntitySnapshotRecord>& data)
{
	data.clear();
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
			data.insert_or_assign(entity.Entity_ID, EntitySnapshotRecord{.boundID = boundID,
																			 .entity = std::move(entity)});
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

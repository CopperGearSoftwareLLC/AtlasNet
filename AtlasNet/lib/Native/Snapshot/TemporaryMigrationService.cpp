#include "TemporaryMigrationService.hpp"

#include <array>
#include <chrono>
#include <execution>
#include <limits>
#include <string>
#include <thread>

#include <sw/redis++/redis.h>

#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Snapshot/Packet/TemporaryMigrationTriggerPacket.hpp"
#include "Snapshot/SnapshotService.hpp"
#include "Transfer/TransferCoordinator.hpp"

namespace
{
constexpr auto kAdoptionScanInterval = std::chrono::milliseconds(250);
constexpr auto kEntityRecoveryLeaseTtl = std::chrono::seconds(5);

std::string SerializeEntityID(const AtlasEntityID& entityID)
{
	return UUIDGen::ToString(entityID);
}

std::string EntityRecoveryLeaseKey(const AtlasEntityID& entityID)
{
	return "Entity:RecoveryLease:" + SerializeEntityID(entityID);
}

std::unordered_set<std::string> CollectLiveShardIDs()
{
	std::vector<std::string> livePeerKeys;
	HealthManifest::Get().GetLivePings(livePeerKeys);

	std::unordered_set<std::string> liveShardIDs;
	liveShardIDs.reserve(livePeerKeys.size());
	for (const std::string& livePeerKey : livePeerKeys)
	{
		ByteReader livePeerReader(livePeerKey);
		NetworkIdentity livePeerIdentity;
		livePeerIdentity.Deserialize(livePeerReader);
		if (livePeerIdentity.Type == NetworkIdentityType::eShard)
		{
			liveShardIDs.insert(UUIDGen::ToString(livePeerIdentity.ID));
		}
	}
	return liveShardIDs;
}

std::vector<NetworkIdentity> CollectLiveShardPeers()
{
	std::vector<std::string> livePeerKeys;
	HealthManifest::Get().GetLivePings(livePeerKeys);

	std::vector<NetworkIdentity> liveShardPeers;
	liveShardPeers.reserve(livePeerKeys.size());
	for (const std::string& livePeerKey : livePeerKeys)
	{
		ByteReader livePeerReader(livePeerKey);
		NetworkIdentity livePeerIdentity;
		livePeerIdentity.Deserialize(livePeerReader);
		if (livePeerIdentity.Type == NetworkIdentityType::eShard)
		{
			liveShardPeers.push_back(std::move(livePeerIdentity));
		}
	}
	return liveShardPeers;
}

bool TryAcquireEntityRecoveryLease(const AtlasEntityID& entityID)
{
	const std::string key = EntityRecoveryLeaseKey(entityID);
	const std::string value = UUIDGen::ToString(NetworkCredentials::Get().GetID().ID);
	const std::string ttlSeconds = std::to_string(kEntityRecoveryLeaseTtl.count());

	const auto response = InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 6> cmd = {"SET", key, value, "NX", "EX", ttlSeconds};
			return r.command(cmd.begin(), cmd.end());
		});

	return response && response->str != nullptr;
}

void ReleaseEntityRecoveryLease(const AtlasEntityID& entityID)
{
	(void)InternalDB::Get()->Del(EntityRecoveryLeaseKey(entityID));
}
}  // namespace

TemporaryMigrationService::TemporaryMigrationService()
{
	migrationTriggerSubscription =
		Interlink::Get().GetPacketManager().Subscribe<TemporaryMigrationTriggerPacket>(
			[this](const TemporaryMigrationTriggerPacket& packet,
				   const PacketManager::PacketInfo& info)
			{
				logger.WarningFormatted(
					"Received temporary migration trigger for released bound {} from {}",
					packet.releasedBoundID, info.sender.ToString());
				RecoverAdoptableEntitiesIfNeeded();
			});
	adoptionThread = std::jthread([this](std::stop_token st) { AdoptionThreadLoop(st); });
}

void TemporaryMigrationService::Shutdown()
{
	migrationTriggerSubscription.Reset();
	if (adoptionThread.joinable())
	{
		adoptionThread.request_stop();
		adoptionThread.join();
	}
}

void TemporaryMigrationService::AdoptionThreadLoop(std::stop_token st)
{
	while (!st.stop_requested())
	{
		RecoverAdoptableEntitiesIfNeeded();

		if (st.stop_requested())
		{
			break;
		}

		std::this_thread::sleep_for(kAdoptionScanInterval);
	}
}

void TemporaryMigrationService::TriggerForCurrentShardSigterm()
{
	std::lock_guard lock(migrationMutex);
	if (!BoundLeaser::Get().HasBound())
	{
		return;
	}

	const NetworkIdentity selfID = NetworkCredentials::Get().GetID();
	const BoundsID boundID = BoundLeaser::Get().GetBoundID();
	SnapshotService::Get().FlushClaimedBoundSnapshot();
	HealthManifest::Get().RemovePingByIdentity(selfID);
	const bool requeued = HeuristicManifest::Get().ReleaseClaimedBound(selfID);
	BoundLeaser::Get().ClearClaimedBound();
	std::vector<AtlasEntityID> entityIDsToDrain;
	EntityLedger::Get().ForEachEntityRead(
		std::execution::seq,
		[&](const AtlasEntity& entity)
		{
			if (TransferCoordinator::Get().IsEntityInTransfer(entity.Entity_ID))
			{
				return;
			}
			entityIDsToDrain.push_back(entity.Entity_ID);
		});
	for (const AtlasEntityID& entityID : entityIDsToDrain)
	{
		if (!EntityLedger::Get().ExistsEntity(entityID))
		{
			continue;
		}
		EntityLedger::Get().EraseEntity(entityID);
	}
	TemporaryMigrationTriggerPacket triggerPacket;
	triggerPacket.releasedBoundID = boundID;
	for (const NetworkIdentity& liveShardPeer : CollectLiveShardPeers())
	{
		if (liveShardPeer == selfID)
		{
			continue;
		}

		Interlink::Get().SendMessage(
			liveShardPeer, triggerPacket, NetworkMessageSendFlag::eReliableNow);
	}

	logger.WarningFormatted(
		"Prepared shard {} and bound {} for temporary migration (requeued={}, drained={})",
		selfID.ToString(), boundID, requeued ? "true" : "false", entityIDsToDrain.size());
}

bool TemporaryMigrationService::TryClaimRecoveredEntityOwnership(
	const AtlasEntityID& entityID, const std::unordered_set<std::string>& liveShardIDs,
	std::optional<BoundsID> requiredUnownedBoundID)
{
	if (!TryAcquireEntityRecoveryLease(entityID))
	{
		return false;
	}

	bool claimed = false;
	const ShardID selfShardID = NetworkCredentials::Get().GetID().ID;
	const std::string selfShardIDStr = UUIDGen::ToString(selfShardID);

	do
	{
		if (requiredUnownedBoundID.has_value())
		{
			const bool stillUnowned = HeuristicManifest::Get().QueryOwnershipState(
				[&](const HeuristicManifest::OwnershipStateWrapper& ownership)
				{
					return !ownership.GetBoundOwner(*requiredUnownedBoundID).has_value();
				});
			if (!stillUnowned)
			{
				break;
			}
		}

		const std::optional<ShardID> ownerShard =
			GlobalEntityLedger::Get().GetEntityOwnerShard(entityID);
		if (ownerShard.has_value() && UUIDGen::ToString(*ownerShard) != selfShardIDStr &&
			liveShardIDs.contains(UUIDGen::ToString(*ownerShard)))
		{
			break;
		}

		GlobalEntityLedger::Get().DeclareEntityRecord(selfShardID, entityID);
		claimed = true;
	} while (false);

	ReleaseEntityRecoveryLease(entityID);
	return claimed;
}

bool TemporaryMigrationService::ShouldCurrentShardAdoptEntity(
	const AtlasEntity& entity, const std::unordered_map<BoundsID, ShardID>& liveClaimedBounds,
	BoundsID claimedBoundID) const
{
	if (!liveClaimedBounds.contains(claimedBoundID) || liveClaimedBounds.empty())
	{
		return false;
	}

	return HeuristicManifest::Get().PullHeuristic(
		[&](const IHeuristic& heuristic)
		{
			float bestDistanceSq = std::numeric_limits<float>::max();
			BoundsID bestBoundID = claimedBoundID;

			for (const auto& [boundID, ownerShardID] : liveClaimedBounds)
			{
				(void)ownerShardID;
				const vec3 center = heuristic.GetBound(boundID).GetCenter();
				const vec3 delta = center - entity.transform.position;
				const float distanceSq = glm::dot(delta, delta);
				if (distanceSq < bestDistanceSq ||
					(distanceSq == bestDistanceSq && boundID < bestBoundID))
				{
					bestDistanceSq = distanceSq;
					bestBoundID = boundID;
				}
			}

			return bestBoundID == claimedBoundID;
		});
}

void TemporaryMigrationService::RecoverAdoptableEntitiesIfNeeded()
{
	std::lock_guard lock(migrationMutex);
	if (!BoundLeaser::Get().HasBound())
	{
		return;
	}

	const BoundsID claimedBoundID = BoundLeaser::Get().GetBoundID();
	const uint32_t boundsCount =
		HeuristicManifest::Get().PullHeuristic(
			[&](const IHeuristic& heuristic) { return heuristic.GetBoundsCount(); });
	const std::unordered_set<std::string> liveShardIDs = CollectLiveShardIDs();
	if (liveShardIDs.empty())
	{
		return;
	}

	const auto claimedBounds = HeuristicManifest::Get().QueryOwnershipState(
		[&](const HeuristicManifest::OwnershipStateWrapper& ownership)
		{
			std::unordered_map<BoundsID, ShardID> result;
			result.reserve(boundsCount);
			for (BoundsID boundID = 0; boundID < boundsCount; ++boundID)
			{
				const std::optional<NetworkIdentity> owner = ownership.GetBoundOwner(boundID);
				if (!owner.has_value())
				{
					continue;
				}
				result.emplace(boundID, owner->ID);
			}
			return result;
		});

	std::unordered_map<BoundsID, ShardID> liveClaimedBounds;
	liveClaimedBounds.reserve(claimedBounds.size());
	for (const auto& [boundID, ownerShardID] : claimedBounds)
	{
		if (liveShardIDs.contains(UUIDGen::ToString(ownerShardID)))
		{
			liveClaimedBounds.emplace(boundID, ownerShardID);
		}
	}

	if (!liveClaimedBounds.contains(claimedBoundID))
	{
		return;
	}

	std::unordered_map<BoundsID, std::vector<AtlasEntity>> snapshotsByBound;
	SnapshotService::Get().FetchEntityListSnapshot(snapshotsByBound);

	size_t adoptedCount = 0;
	for (const auto& [snapshotBoundID, entities] : snapshotsByBound)
	{
		if (snapshotBoundID == claimedBoundID || claimedBounds.contains(snapshotBoundID))
		{
			continue;
		}

		for (const AtlasEntity& entity : entities)
		{
			if (EntityLedger::Get().ExistsEntity(entity.Entity_ID) ||
				!ShouldCurrentShardAdoptEntity(entity, liveClaimedBounds, claimedBoundID))
			{
				continue;
			}

			const std::optional<ShardID> ownerShard =
				GlobalEntityLedger::Get().GetEntityOwnerShard(entity.Entity_ID);
			if (ownerShard.has_value() && liveShardIDs.contains(UUIDGen::ToString(*ownerShard)))
			{
				continue;
			}

			if (!TryClaimRecoveredEntityOwnership(entity.Entity_ID, liveShardIDs,
												  snapshotBoundID))
			{
				continue;
			}
			if (EntityLedger::Get().ExistsEntity(entity.Entity_ID))
			{
				continue;
			}

			EntityLedger::Get().AddEntity(entity);
			++adoptedCount;
		}
	}

	if (adoptedCount > 0)
	{
		logger.WarningFormatted(
			"Temporarily adopted {} orphaned entities into claimed bound {} while waiting for "
			"replacement shard ownership",
			adoptedCount, claimedBoundID);
	}
}

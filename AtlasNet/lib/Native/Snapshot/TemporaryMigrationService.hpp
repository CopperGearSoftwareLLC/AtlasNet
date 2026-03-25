#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IBounds.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

class TemporaryMigrationService : public Singleton<TemporaryMigrationService>
{
	Log logger = Log("TemporaryMigrationService");
	std::jthread adoptionThread;
	std::atomic_bool migrationInProgress = false;
	std::mutex migrationMutex;
	PacketManager::Subscription migrationTriggerSubscription;

   public:
	TemporaryMigrationService();
	bool IsMigrationInProgress() const
	{
		return migrationInProgress.load(std::memory_order_acquire);
	}
	void TriggerForCurrentShardSigterm();
	void Shutdown();

   private:
	void AdoptionThreadLoop(std::stop_token st);
	void RecoverAdoptableEntitiesIfNeeded();
	bool TryClaimRecoveredEntityOwnership(const AtlasEntityID& entityID,
										  const std::unordered_set<std::string>& liveShardIDs);
	bool ShouldCurrentShardAdoptEntity(
		const AtlasEntity& entity,
		const std::unordered_map<BoundsID, ShardID>& liveClaimedBounds,
		BoundsID claimedBoundID) const;
};

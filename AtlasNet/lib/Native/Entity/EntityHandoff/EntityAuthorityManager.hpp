#pragma once

// Coordinates shard-side authority lifecycle: ownership selection, trigger checks,
// and delegation to tracker/simulator/packet components.

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "Entity/EntityHandoff/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/EntityAuthorityTracker.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class EntityAuthorityManager : public Singleton<EntityAuthorityManager>
{
  public:
	struct PendingIncomingHandoff
	{
		AtlasEntity entity;
		NetworkIdentity sender;
		uint64_t transferTick = 0;
	};

	struct PendingOutgoingHandoff
	{
		AtlasEntityID entityId = 0;
		NetworkIdentity targetIdentity;
		std::string targetClaimKey;
		uint64_t transferTick = 0;
	};

	EntityAuthorityManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);
	void OnIncomingHandoffEntityAtTick(const AtlasEntity& entity,
									   const NetworkIdentity& sender,
									   uint64_t transferTick);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void EvaluateTestEntityOwnership();
	void EvaluateHeuristicPositionTriggers();
	void ProcessOutgoingHandoffCommit();

		NetworkIdentity selfIdentity;
		std::shared_ptr<Log> logger;
		bool initialized = false;
		bool isTestEntityOwner = false;
		bool ownershipEvaluated = false;
		bool hasOwnershipLogState = false;
		bool lastOwnershipState = false;
		std::unique_ptr<EntityAuthorityTracker> tracker;
		std::unique_ptr<DebugEntityOrbitSimulator> debugSimulator;
		std::optional<PendingIncomingHandoff> pendingIncomingEntity;
		std::optional<PendingOutgoingHandoff> pendingOutgoingHandoff;
		std::unordered_map<AtlasEntityID, uint64_t>
			adoptionHandoffCooldownUntilTick;
		uint64_t localAuthorityTick = 0;
		std::chrono::steady_clock::time_point lastTickTime;
		std::chrono::steady_clock::time_point lastOwnerEvalTime;
		std::chrono::steady_clock::time_point lastSnapshotTime;
};

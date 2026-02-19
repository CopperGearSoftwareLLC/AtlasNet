#pragma once

// Coordinates shard-side authority lifecycle: ownership selection, trigger checks,
// and delegation to tracker/simulator/packet components.

#include <chrono>
#include <memory>
#include <optional>

#include "Entity/EntityHandoff/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/EntityAuthorityTracker.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class EntityAuthorityManager : public Singleton<EntityAuthorityManager>
{
  public:
	EntityAuthorityManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void EvaluateTestEntityOwnership();
	void EvaluateHeuristicPositionTriggers();

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	bool isTestEntityOwner = false;
	bool ownershipEvaluated = false;
	bool hasOwnershipLogState = false;
	bool lastOwnershipState = false;
	std::unique_ptr<EntityAuthorityTracker> tracker;
	std::unique_ptr<DebugEntityOrbitSimulator> debugSimulator;
	std::optional<AtlasEntity> pendingIncomingEntity;
	std::chrono::steady_clock::time_point lastTickTime;
	std::chrono::steady_clock::time_point lastOwnerEvalTime;
	std::chrono::steady_clock::time_point lastSnapshotTime;
};

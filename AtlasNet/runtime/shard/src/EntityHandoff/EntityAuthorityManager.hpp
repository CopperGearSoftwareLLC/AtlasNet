#pragma once

#include <chrono>
#include <memory>

#include "EntityHandoff/DebugEntityOrbitSimulator.hpp"
#include "EntityHandoff/EntityAuthorityTracker.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class EntityAuthorityManager : public Singleton<EntityAuthorityManager>
{
  public:
	EntityAuthorityManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void EvaluateTestEntityOwnership();

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	bool isTestEntityOwner = false;
	bool ownershipEvaluated = false;
	std::unique_ptr<EntityAuthorityTracker> tracker;
	std::unique_ptr<DebugEntityOrbitSimulator> debugSimulator;
	std::chrono::steady_clock::time_point lastTickTime;
	std::chrono::steady_clock::time_point lastOwnerEvalTime;
	std::chrono::steady_clock::time_point lastSnapshotTime;
};

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntityOrbitSimulator;
class Log;
class NH_EntityAuthorityTracker;
class SH_BorderHandoffPlanner;
class SH_OwnershipElection;
class SH_TelemetryPublisher;
class SH_TransferMailbox;

class SH_ServerAuthorityRuntime
{
  public:
	SH_ServerAuthorityRuntime();
	~SH_ServerAuthorityRuntime();

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
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	bool hasSeededInitialEntities = false;
	uint64_t localAuthorityTick = 0;
	std::chrono::steady_clock::time_point lastTickTime;
	std::chrono::steady_clock::time_point lastSnapshotTime;
	std::unique_ptr<NH_EntityAuthorityTracker> tracker;
	std::unique_ptr<DebugEntityOrbitSimulator> debugSimulator;
	std::unique_ptr<SH_OwnershipElection> ownershipElection;
	std::unique_ptr<SH_BorderHandoffPlanner> borderPlanner;
	std::unique_ptr<SH_TransferMailbox> transferMailbox;
	std::unique_ptr<SH_TelemetryPublisher> telemetryPublisher;
};

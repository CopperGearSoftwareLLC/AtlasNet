#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntitySimulator;
class Log;
class SH_BorderHandoffPlanner;
class SH_EntityAuthorityTracker;
class SH_OwnershipElection;
class SH_TelemetryPublisher;
class SH_TransferMailbox;

// Main per-shard runtime for ServerHandoff.
//
// High-level tick order:
// 1) adopt due incoming handoffs
// 2) simulate local entities
// 3) plan/send outgoing handoffs
// 4) commit due outgoing handoffs
// 5) publish telemetry
class SH_ServerAuthorityRuntime
{
  public:
	SH_ServerAuthorityRuntime();
	~SH_ServerAuthorityRuntime();

	// Lifecycle
	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();

	// Network handoff ingress
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);
	void OnIncomingHandoffEntityAtTimeUs(const AtlasEntity& entity,
										 const NetworkIdentity& sender,
										 uint64_t transferTimeUs);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	bool hasSeededInitialEntities = false;
	std::chrono::steady_clock::time_point lastTickTime;
	std::chrono::steady_clock::time_point lastSnapshotTime;
	std::unique_ptr<SH_EntityAuthorityTracker> tracker;
	std::unique_ptr<DebugEntitySimulator> debugSimulator;
	std::unique_ptr<SH_OwnershipElection> ownershipElection;
	std::unique_ptr<SH_BorderHandoffPlanner> borderPlanner;
	std::unique_ptr<SH_TransferMailbox> transferMailbox;
	std::unique_ptr<SH_TelemetryPublisher> telemetryPublisher;
};

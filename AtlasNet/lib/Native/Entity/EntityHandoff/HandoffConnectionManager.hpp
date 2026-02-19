#pragma once

// Manages handoff transport connection lifecycle and inactivity cleanup, while
// delegating lease semantics to the lease coordinator.

#include <memory>
#include <unordered_set>

#include "Entity/EntityHandoff/HandoffConnectionLeaseCoordinator.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class HandoffConnectionManager : public Singleton<HandoffConnectionManager>
{
  public:
	HandoffConnectionManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();
	void MarkConnectionActivity(const NetworkIdentity& peer);
	void SetLeaseModeEnabled(bool enabled);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	std::unordered_set<NetworkIdentity> activeConnections;
	bool leaseModeEnabled = true;
	std::unique_ptr<HandoffConnectionLeaseCoordinator> leaseCoordinator;
};

#pragma once

// Tracks handoff peer connections.
// Handles peer activity and timeout cleanup.

#include <memory>
#include <unordered_set>

#include "NH_HandoffConnectionLeaseCoordinator.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class NH_HandoffConnectionManager : public Singleton<NH_HandoffConnectionManager>
{
  public:
	NH_HandoffConnectionManager() = default;

	// Lifecycle
	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();

	// Peer activity + lease options
	void MarkConnectionActivity(const NetworkIdentity& peer);
	void SetLeaseModeEnabled(bool enabled);

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	std::unordered_set<NetworkIdentity> activeConnections;
	bool leaseModeEnabled = true;
	std::unique_ptr<NH_HandoffConnectionLeaseCoordinator> leaseCoordinator;
};

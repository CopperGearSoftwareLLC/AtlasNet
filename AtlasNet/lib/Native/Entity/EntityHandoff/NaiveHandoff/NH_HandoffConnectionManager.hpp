#pragma once

// NH naive handoff transport connection manager.
// Owns peer-connection lifecycle and delegates lease semantics to the lease
// coordinator component.

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
	std::unique_ptr<NH_HandoffConnectionLeaseCoordinator> leaseCoordinator;
};

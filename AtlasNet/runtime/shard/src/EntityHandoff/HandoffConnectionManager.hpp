#pragma once

#include <memory>
#include <unordered_set>

#include "EntityHandoff/HandoffConnectionLeaseCoordinator.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
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

// SH handoff connection manager implementation.
// Manages peer connection lifecycle and activity-driven pruning/reap behavior.

#include "SH_HandoffConnectionManager.hpp"

#include "Interlink/Interlink.hpp"

void SH_HandoffConnectionManager::Init(const NetworkIdentity& self,
									std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	activeConnections.clear();
	leaseCoordinator = std::make_unique<SH_HandoffConnectionLeaseCoordinator>(
		selfIdentity, logger, SH_HandoffConnectionLeaseCoordinator::Options{});
	leaseCoordinator->SetLeaseEnabled(leaseModeEnabled);
	initialized = true;

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] SH_HandoffConnectionManager initialized for {}",
			selfIdentity.ToString());
	}
}

void SH_HandoffConnectionManager::Tick()
{
	if (!initialized || !leaseCoordinator)
	{
		return;
	}

	leaseCoordinator->ReapInactiveConnections(
		std::chrono::steady_clock::now(),
		[this](const NetworkIdentity& peer, const std::chrono::seconds inactiveFor)
		{
			Interlink::Get().CloseConnectionTo(peer, 0,
											   "EntityHandoff inactivity timeout");
			activeConnections.erase(peer);
			if (logger)
			{
				logger->WarningFormatted(
					"[EntityHandoff] Reaped inactive handoff connection {} "
					"(inactive={}s)",
					peer.ToString(), inactiveFor.count());
			}
		});
}

void SH_HandoffConnectionManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	for (const auto& peer : activeConnections)
	{
		Interlink::Get().CloseConnectionTo(peer, 0, "EntityHandoff shutdown");
	}
	activeConnections.clear();

	if (leaseCoordinator)
	{
		leaseCoordinator->Clear();
		leaseCoordinator.reset();
	}

	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] SH_HandoffConnectionManager shutdown");
	}
}

void SH_HandoffConnectionManager::MarkConnectionActivity(const NetworkIdentity& peer)
{
	if (!initialized)
	{
		return;
	}

	activeConnections.insert(peer);
	if (leaseCoordinator)
	{
		leaseCoordinator->MarkConnectionActivity(peer);
	}
}

void SH_HandoffConnectionManager::SetLeaseModeEnabled(bool enabled)
{
	leaseModeEnabled = enabled;
	if (leaseCoordinator)
	{
		leaseCoordinator->SetLeaseEnabled(enabled);
	}
}

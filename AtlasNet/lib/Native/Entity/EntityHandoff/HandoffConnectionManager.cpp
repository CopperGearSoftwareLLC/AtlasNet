// Implements handoff connection init/tick/shutdown and activity-driven pruning.

#include "Entity/EntityHandoff/HandoffConnectionManager.hpp"

#include "Entity/EntityHandoff/HandoffPacketManager.hpp"
#include "Interlink/Interlink.hpp"

void HandoffConnectionManager::Init(const NetworkIdentity& self,
									std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	activeConnections.clear();
	leaseCoordinator = std::make_unique<HandoffConnectionLeaseCoordinator>(
		selfIdentity, logger, HandoffConnectionLeaseCoordinator::Options{});
	leaseCoordinator->SetLeaseEnabled(leaseModeEnabled);
	HandoffPacketManager::Get().Init(selfIdentity, logger);
	initialized = true;

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffConnectionManager initialized for {}",
			selfIdentity.ToString());
	}
}

void HandoffConnectionManager::Tick()
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

void HandoffConnectionManager::Shutdown()
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

	HandoffPacketManager::Get().Shutdown();

	if (leaseCoordinator)
	{
		leaseCoordinator->Clear();
		leaseCoordinator.reset();
	}

	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] HandoffConnectionManager shutdown");
	}
}

void HandoffConnectionManager::MarkConnectionActivity(const NetworkIdentity& peer)
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

void HandoffConnectionManager::SetLeaseModeEnabled(bool enabled)
{
	leaseModeEnabled = enabled;
	if (leaseCoordinator)
	{
		leaseCoordinator->SetLeaseEnabled(enabled);
	}
}

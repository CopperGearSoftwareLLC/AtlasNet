#include "EntityHandoff/HandoffConnectionManager.hpp"

#include "EntityHandoff/HandoffPacketManager.hpp"
#include "Interlink.hpp"

void HandoffConnectionManager::Init(const NetworkIdentity& self,
									std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	activeConnections.clear();
	leaseCoordinator = std::make_unique<HandoffConnectionLeaseCoordinator>(
		selfIdentity, logger,
		HandoffConnectionLeaseCoordinator::Options{
			.leaseEnabled = leaseModeEnabled});

	HandoffPacketManager::Get().Init(selfIdentity, logger);

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffConnectionManager initialized for {}",
			selfIdentity.ToString());
		const auto& options = leaseCoordinator->GetOptions();
		logger->DebugFormatted(
			"[EntityHandoff] Lease mode={} inactivity_timeout={}s",
			leaseModeEnabled ? "enabled" : "disabled",
			std::chrono::duration_cast<std::chrono::seconds>(
				options.inactivityTimeout)
				.count());
		logger->Debug(
			"[EntityHandoff] Random probe/connect test disabled");
	}
}

void HandoffConnectionManager::Tick()
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (leaseCoordinator)
	{
		leaseCoordinator->ReapInactiveConnections(
			now,
			[this](const NetworkIdentity& peer, std::chrono::seconds idleFor)
			{
				Interlink::Get().CloseConnectionTo(
					peer, 0, "EntityHandoff inactivity timeout");
				activeConnections.erase(peer);
				if (logger)
				{
					logger->WarningFormatted(
						"[EntityHandoff] Closed inactive connection to {} idle={}s",
						peer.ToString(), idleFor.count());
				}
			});
	}

}

void HandoffConnectionManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	for (const auto& peer : activeConnections)
	{
		Interlink::Get().CloseConnectionTo(peer, 0,
										   "EntityHandoff shutdown");
		if (leaseCoordinator)
		{
			leaseCoordinator->ReleaseLeaseIfOwned(peer);
		}
	}

	activeConnections.clear();
	if (leaseCoordinator)
	{
		leaseCoordinator->Clear();
		leaseCoordinator.reset();
	}
	HandoffPacketManager::Get().Shutdown();
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

	if (leaseCoordinator)
	{
		leaseCoordinator->MarkConnectionActivity(peer);
		activeConnections.insert(peer);
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

#include "Partition.hpp"

#include <chrono>
#include <thread>

#include "Interlink/Database/HealthManifest.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Entity/EntityHandoff/EntityAuthorityManager.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Interlink.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
Partition::Partition() {}
Partition::~Partition() {}

namespace
{
constexpr std::chrono::milliseconds kPartitionTickInterval(32);
}

void Partition::Init()
{
	NetworkIdentity partitionIdentifier(NetworkIdentityType::eShard,
										UUIDGen::Gen());

	logger = std::make_shared<Log>(partitionIdentifier.ToString());

	HealthManifest::Get().ScheduleHealthPings(partitionIdentifier);
	NetworkManifest::Get().ScheduleNetworkPings(partitionIdentifier);
	Interlink::Get().Init(
		InterlinkProperties{.ThisID = partitionIdentifier, .logger = logger});
	

	{
		GridShape claimedBounds;
		const bool claimed =
			HeuristicManifest::Get().ClaimNextPendingBound<GridShape>(
				partitionIdentifier.ToString(), claimedBounds);
		if (claimed)
		{
			logger->DebugFormatted("Claimed bounds {} for shard",
								   claimedBounds.GetID());
		}
		else
		{
			logger->Warning("No pending bounds available to claim");
		}
	}
	// Clear any existing partition entity data to prevent stale data
	while (!ShouldShutdown)
	{
		std::this_thread::sleep_for(kPartitionTickInterval);	// ~30 updates/sec
		EntityAuthorityManager::Get().Tick();
	}

	logger->Debug("Shutting Down");
	EntityAuthorityManager::Get().Shutdown();
	Interlink::Get().Shutdown();
}

#include "Partition.hpp"

#include <chrono>
#include <thread>

#include "Interlink/Database/HealthManifest.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityManager.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Interlink.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
Partition::Partition() {}
Partition::~Partition() {}

namespace
{
constexpr std::chrono::milliseconds kPartitionTickInterval(32);
constexpr std::chrono::seconds kDefaultClaimRetryInterval(1);
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

	auto tryClaimBound = [&]() -> bool
	{
		GridShape claimedBounds;
		const bool claimed =
			HeuristicManifest::Get().ClaimNextPendingBound<GridShape>(
				partitionIdentifier, claimedBounds);
		if (claimed)
		{
			logger->DebugFormatted("Claimed bounds {} for shard",
								   claimedBounds.GetID());
			return true;
		}
		return false;
	};

	bool hasClaimedBound = tryClaimBound();
	auto nextClaimAttemptTime =
		std::chrono::steady_clock::now() + kDefaultClaimRetryInterval;

	if (!hasClaimedBound)
	{
		logger->WarningFormatted(
			"No pending bounds available to claim for shard={} (pending={} claimed={}). "
			"Will retry every {}s.",
			partitionIdentifier.ToString(),
			HeuristicManifest::Get().GetPendingBoundsCount(),
			HeuristicManifest::Get().GetClaimedBoundsCount(),
			kDefaultClaimRetryInterval.count());
	}
	// Clear any existing partition entity data to prevent stale data
	while (!ShouldShutdown)
	{
		const auto now = std::chrono::steady_clock::now();
		if (!hasClaimedBound && now >= nextClaimAttemptTime)
		{
			hasClaimedBound = tryClaimBound();
			nextClaimAttemptTime = now + kDefaultClaimRetryInterval;
			if (!hasClaimedBound)
			{
				logger->DebugFormatted(
					"Retrying bound claim for shard={} (pending={} claimed={})",
					partitionIdentifier.ToString(),
					HeuristicManifest::Get().GetPendingBoundsCount(),
					HeuristicManifest::Get().GetClaimedBoundsCount());
			}
		}
		std::this_thread::sleep_for(kPartitionTickInterval);	// ~30 updates/sec
		NH_EntityAuthorityManager::Get().Tick();
	}

	logger->Debug("Shutting Down");
	NH_EntityAuthorityManager::Get().Shutdown();
	Interlink::Get().Shutdown();
}

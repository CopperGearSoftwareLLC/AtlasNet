#include "AtlasNetServer.hpp"

#include <chrono>
#include <stop_token>
#include <thread>

#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
#include "Entity/EntityHandoff/ServerHandoff/SH_ServerAuthorityManager.hpp"
#include "Events/EventEnums.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Debug/LogEvent.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
constexpr std::chrono::seconds kDefaultClaimRetryInterval(1);
}

// ============================================================================
// Initialize server and setup Interlink callbacks
// ============================================================================
void AtlasNetServer::Initialize(
	AtlasNetServer::InitializeProperties &properties)
{
	CrashHandler::Get().Init();
	identity = NetworkIdentity(NetworkIdentityType::eShard, UUIDGen::Gen());
	NetworkManifest::Get().ScheduleNetworkPings(identity);
	HealthManifest::Get().ScheduleHealthPings(identity);
	EventSystem::Get().Init(identity);
	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});
	EventSystem::Get().Subscribe<LogEvent>(
		[&](const LogEvent &e)
		{
			logger->DebugFormatted("Received LogEvent: {}", e.message);
		});
	logger->Debug("AtlasNet Initialize");
	// --- Interlink setup ---
	Interlink::Get().Init({
		.ThisID = identity,
		.logger = logger,
	});

	ShardLogicThread =
		std::jthread([&](std::stop_token st) { ShardLogicEntry(st); });
}

// ============================================================================
// Update: Called every tick
// Sends entity updates, incoming, and outgoing data to Partition
// ============================================================================
void AtlasNetServer::Update(std::span<AtlasEntity> entities,
							std::vector<AtlasEntity> &IncomingEntities,
							std::vector<AtlasEntityID> &OutgoingEntities)
{
}
void AtlasNetServer::ShardLogicEntry(std::stop_token st)
{
	auto tryClaimBound = [this]() -> bool
	{
		GridShape claimedBounds;
		const bool claimed =
			HeuristicManifest::Get().ClaimNextPendingBound<GridShape>(
				identity, claimedBounds);
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

	if (!hasClaimedBound && logger)
	{
		logger->WarningFormatted(
			"No pending bounds available to claim for shard={} (pending={} claimed={}). "
			"Will retry every {}s.",
			identity.ToString(), HeuristicManifest::Get().GetPendingBoundsCount(),
			HeuristicManifest::Get().GetClaimedBoundsCount(),
			kDefaultClaimRetryInterval.count());
	}

	SH_ServerAuthorityManager::Get().Init(identity, logger);
	while (!st.stop_requested())
	{
		const auto now = std::chrono::steady_clock::now();
		if (!hasClaimedBound && now >= nextClaimAttemptTime)
		{
			hasClaimedBound = tryClaimBound();
			nextClaimAttemptTime = now + kDefaultClaimRetryInterval;
			if (!hasClaimedBound && logger)
			{
				logger->DebugFormatted(
					"Retrying bound claim for shard={} (pending={} claimed={})",
					identity.ToString(),
					HeuristicManifest::Get().GetPendingBoundsCount(),
					HeuristicManifest::Get().GetClaimedBoundsCount());
			}
		}

		// Interlink::Get().Tick();
		SH_ServerAuthorityManager::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	SH_ServerAuthorityManager::Get().Shutdown();
}

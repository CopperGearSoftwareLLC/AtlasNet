#include "AtlasNetServer.hpp"

#include <stop_token>
#include <thread>

#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
#include "Entity/EntityHandoff/EntityAuthorityManager.hpp"
#include "Events/EventEnums.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Debug/LogEvent.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkIdentity.hpp"
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
	GridShape claimedBounds;
		const bool claimed =
			HeuristicManifest::Get().ClaimNextPendingBound<GridShape>(
				identity, claimedBounds);
		if (claimed)
		{
			logger->DebugFormatted("Claimed bounds {} for shard",
								   claimedBounds.GetID());
		}
		else
		{
			logger->Warning("No pending bounds available to claim");
		}
		
	EntityAuthorityManager::Get().Init(identity, logger);
	while (!st.stop_requested())
	{
		// Interlink::Get().Tick();
		EntityAuthorityManager::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

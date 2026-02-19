#include "AtlasNetServer.hpp"

#include <stop_token>
#include <thread>

#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
#include "Entity/EntityHandoff/EntityAuthorityManager.hpp"
#include "Global/Misc/UUID.hpp"
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

	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});

	logger = std::make_shared<Log>("Shard");
	logger->Debug("AtlasNet Initialize");
	// --- Interlink setup ---
	Interlink::Get().Init({
		.ThisID = identity,
		.logger = logger,
	});

	logger->Debug("Interlink initialized");

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
	EntityAuthorityManager::Get().Init(identity, logger);
	while (!st.stop_requested())
	{
		// Interlink::Get().Tick();
		EntityAuthorityManager::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

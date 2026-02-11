#include "AtlasNetServer.hpp"

#include "BuiltInDB.hpp"
#include "Crash/CrashHandler.hpp"
#include "DockerIO.hpp"
#include "Misc/UUID.hpp"
// ============================================================================
// Initialize server and setup Interlink callbacks
// ============================================================================
void AtlasNetServer::Initialize(
	AtlasNetServer::InitializeProperties &properties)
{
	// --- Core setup ---
	// CrashHandler::Get().Init();
	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});

	NetworkIdentity myID(NetworkIdentityType::eGameServer, UUIDGen::Gen());
	logger = std::make_shared<Log>(myID.ToString());
	logger->Debug("AtlasNet Initialize");
	// --- Interlink setup ---
	Interlink::Get().Init(
		{.ThisID = myID,
		 .logger = logger,});

	logger->Debug(
		"Interlink initialized; waiting for auto-connect to Partition...");
}

// ============================================================================
// Update: Called every tick
// Sends entity updates, incoming, and outgoing data to Partition
// ============================================================================
void AtlasNetServer::Update(
	std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
	std::vector<AtlasEntity::EntityID> &OutgoingEntities)
{
	
}

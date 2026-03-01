#include "AtlasNetServer.hpp"

#include <chrono>
#include <stop_token>
#include <thread>

#include "Client/Client.hpp"
#include "Client/Shard/ClientLedger.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
// #include "Entity/EntityHandoff/ServerHandoff/SH_ServerAuthorityManager.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventEnums.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Events/Events/Debug/LogEvent.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"

// namespace
//{
// constexpr std::chrono::seconds kDefaultClaimRetryInterval(1);
// }

// ============================================================================
// Initialize server and setup Interlink callbacks
// ============================================================================
void IAtlasNetServer::AtlasNet_Initialize()
{
	CrashHandler::Get().Init();
	NetworkCredentials::Make(NetworkIdentity(NetworkIdentityType::eShard, UUIDGen::Gen()));
	Interlink::Get().Init();
	NetworkManifest::Get().ScheduleNetworkPings();
	HealthManifest::Get().ScheduleHealthPings();
	EventSystem::Get().Init();
	DockerEvents::Get().Init(DockerEventsInit{});
	EventSystem::Get().Subscribe<LogEvent>(
		[&](const LogEvent &e) { logger->DebugFormatted("Received LogEvent: {}", e.message); });

	BoundLeaser::Get().Init();
	// --- Interlink setup ---

	EntityLedger::Get().Init();
	ClientLedger::Ensure();
	EventSystem::Get().Subscribe<ClientConnectEvent>(
		[this](const ClientConnectEvent &e)
		{
			if (e.ConnectedShard == NetworkCredentials::Get().GetID())
			{
				ClientSpawnInfo i;
				i.client = e.client;
				i.spawnLocation = e.SpawnLocation;
				OnClientSpawn(i);
			}
		});
	logger->Debug("AtlasNet Initialize");
}

AtlasEntityHandle IAtlasNetServer::AtlasNet_CreateEntity(const Transform &t,
														 std::span<const uint8_t> metadata)
{
	AtlasEntity e = Internal_CreateEntity(t, metadata);
	EntityLedger::Get().AddEntity(e);
	AtlasEntityHandle H;
	return H;
}
AtlasEntityHandle IAtlasNetServer::AtlasNet_CreateClientEntity(ClientID c_id, const Transform &t,
															   std::span<const uint8_t> metadata)
{
	AtlasEntity e = Internal_CreateEntity(t, metadata);
	e.Client_ID = c_id;
	e.IsClient = true;
	EntityLedger::Get().AddEntity(e);
	AtlasEntityHandle H;
	return H;
}
AtlasEntity IAtlasNetServer::Internal_CreateEntity(const Transform &t,
												   std::span<const uint8_t> metadata)
{
	AtlasEntity e;
	e.Entity_ID = AtlasEntity::CreateUniqueID();
	e.transform = t;
	e.payload.assign(metadata.begin(), metadata.end());
	return e;
}
void IAtlasNetServer::AtlasNet_SyncEntities(std::span<const AtlasEntityID> ActiveEntities,
											std::span<const AtlasEntityID> &ReleasedEntities,
											std::span<const AtlasEntityHandle> &AcquiredEntities)
{
}

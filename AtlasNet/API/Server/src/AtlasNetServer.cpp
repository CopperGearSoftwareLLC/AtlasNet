#include "AtlasNetServer.hpp"

#include <chrono>
#include <stop_token>
#include <thread>

#include "Client/Client.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
// #include "Entity/EntityHandoff/ServerHandoff/SH_ServerAuthorityManager.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventEnums.hpp"
#include "Events/EventSystem.hpp"
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
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties &properties)
{
	CrashHandler::Get().Init();
	NetworkCredentials::Make(NetworkIdentity(NetworkIdentityType::eShard, UUIDGen::Gen()));
	Interlink::Get().Init();
	NetworkManifest::Get().ScheduleNetworkPings();
	HealthManifest::Get().ScheduleHealthPings();
	EventSystem::Get().Init();
	DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});
	EventSystem::Get().Subscribe<LogEvent>(
		[&](const LogEvent &e) { logger->DebugFormatted("Received LogEvent: {}", e.message); });

	BoundLeaser::Get().Init();
	// --- Interlink setup ---

	EntityLedger::Get().Init();
	logger->Debug("AtlasNet Initialize");

	ShardLogicThread = std::jthread([&](std::stop_token st) { ShardLogicEntry(st); });
}

void AtlasNetServer::ShardLogicEntry(std::stop_token st)
{
	/*
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

	SH_ServerAuthorityManager::Get().Init(identity, logger);*/
	while (!st.stop_requested())
	{
		/* const auto now = std::chrono::steady_clock::now();
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
		SH_ServerAuthorityManager::Get().Tick(); */
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	// SH_ServerAuthorityManager::Get().Shutdown();
}
AtlasEntityHandle AtlasNetServer::CreateEntity(const Transform &t,
											   std::span<const uint8_t> metadata)
{
	AtlasEntity e = Internal_CreateEntity(t, metadata);
	EntityLedger::Get().RegisterNewEntity(e);
	AtlasEntityHandle H;
	return H;
}
AtlasEntityHandle AtlasNetServer::CreateClientEntity(ClientID c_id, const Transform &t,
													 std::span<const uint8_t> metadata)
{
	AtlasEntity e = Internal_CreateEntity(t, metadata);
	e.Client_ID = c_id;
	e.IsClient = true;
	EntityLedger::Get().RegisterNewEntity(e);
	AtlasEntityHandle H;
	return H;
}
AtlasEntity AtlasNetServer::Internal_CreateEntity(const Transform &t,
												  std::span<const uint8_t> metadata)
{
	AtlasEntity e;
	e.Entity_ID = AtlasEntity::CreateUniqueID();
	e.data.transform = t;
	e.Metadata.assign(metadata.begin(), metadata.end());
	return e;
}
void AtlasNetServer::SyncEntities(std::span<const AtlasEntityID> ActiveEntities,
								  std::span<const AtlasEntityID> &ReleasedEntities,
								  std::span<const AtlasEntityHandle> &AcquiredEntities)
{
}

#include "SandboxServer.hpp"

#include <array>
#include <chrono>
#include <execution>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Client/Database/ClientManifest.hpp"
#include "Client/Shard/ClientLedger.hpp"
#include "Command/NetCommand.hpp"
#include "Commands/GameClientInputCommand.hpp"
#include "Commands/GameStateCommand.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Entity/Transform.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Events/GlobalEvents.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Snapshot/SnapshotService.hpp"

namespace
{
constexpr int kInitialSandboxEntityCount = 100;
constexpr std::string_view kInitialSandboxSeedLeaseKey = "Sandbox:InitialEntitySeedLease";
constexpr auto kInitialSandboxSeedLeaseTtl = std::chrono::seconds(30);

bool TryAcquireInitialSandboxSeedLease()
{
	const std::string selfShardID = UUIDGen::ToString(NetworkCredentials::Get().GetID().ID);
	const std::string ttlSeconds = std::to_string(kInitialSandboxSeedLeaseTtl.count());

	const auto response = InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 6> cmd = {"SET", std::string(kInitialSandboxSeedLeaseKey),
											  selfShardID, "NX", "EX", ttlSeconds};
			return r.command(cmd.begin(), cmd.end());
		});
	return response && response->str != nullptr;
}

bool RefreshInitialSandboxSeedLeaseIfOwner()
{
	const std::string selfShardID = UUIDGen::ToString(NetworkCredentials::Get().GetID().ID);
	const std::optional<std::string> leaseOwner =
		InternalDB::Get()->Get(kInitialSandboxSeedLeaseKey);
	if (!leaseOwner.has_value() || leaseOwner.value() != selfShardID)
	{
		return false;
	}

	(void)InternalDB::Get()->Expire(kInitialSandboxSeedLeaseKey, kInitialSandboxSeedLeaseTtl);
	return true;
}
}  // namespace

void SandboxServer::Run()
{
	AtlasNet_Initialize();

	GetCommandBus().Subscribe<GameClientInputCommand>(
		[this](const NetClientIntentHeader& h, const GameClientInputCommand& c)
		{ OnGameClientInputCommand(h, c); });

	GlobalEvents::Get().Subscribe<ClientConnectEvent>(
		[&](const ClientConnectEvent& e)
		{
			logger.DebugFormatted("Client Connected event!\n - ID: {}\n - IP: {}\n - Proxy: {}",
								  UUIDGen::ToString(e.client.ID), e.client.ip.ToString(),
								  e.ConnectedProxy.ToString());
		});

	using InitClock = std::chrono::steady_clock;
	std::mutex shardTransferProbeMutex;
	std::unordered_map<NetworkIdentity, InitClock::time_point> lastShardTransferProbeAt;
	std::unordered_map<NetworkIdentity, InitClock::time_point> lastShardTransferResponseAt;
	InitClock::time_point lastTransferReadinessLogAt = InitClock::time_point::min();
	const auto shardTransferProbeSubscription =
		Interlink::Get().GetPacketManager().Subscribe<LocalEntityListRequestPacket>(
			[&](const LocalEntityListRequestPacket& packet, const PacketManager::PacketInfo& info)
			{
				if (packet.status != LocalEntityListRequestPacket::MsgStatus::eResponse ||
					info.sender.Type != NetworkIdentityType::eShard ||
					info.sender == NetworkCredentials::Get().GetID())
				{
					return;
				}

				std::lock_guard<std::mutex> lock(shardTransferProbeMutex);
				lastShardTransferResponseAt[info.sender] = InitClock::now();
			});

	const auto areShardTransfersReadyForBootstrap =
		[&]() -> bool
		{
			const NetworkIdentity selfID = NetworkCredentials::Get().GetID();
			std::vector<std::string> livePeerKeys;
			HealthManifest::Get().GetLivePings(livePeerKeys);

			std::vector<NetworkIdentity> liveShardPeers;
			liveShardPeers.reserve(livePeerKeys.size());
			for (const std::string& livePeerKey : livePeerKeys)
			{
				ByteReader livePeerReader(livePeerKey);
				NetworkIdentity livePeerIdentity;
				livePeerIdentity.Deserialize(livePeerReader);
				if (livePeerIdentity.Type != NetworkIdentityType::eShard || livePeerIdentity == selfID)
				{
					continue;
				}
				liveShardPeers.push_back(livePeerIdentity);
			}

			if (liveShardPeers.empty())
			{
				return true;
			}

			LocalEntityListRequestPacket probePacket;
			probePacket.status = LocalEntityListRequestPacket::MsgStatus::eQuery;
			probePacket.Request_IncludeMetadata = false;

			const auto now = InitClock::now();
			bool allPeersReady = true;
			size_t readyPeerCount = 0;

			for (const auto& peerID : liveShardPeers)
			{
				bool sendProbe = false;
				bool peerReady = false;
				{
					std::lock_guard<std::mutex> lock(shardTransferProbeMutex);
					const auto probeIt = lastShardTransferProbeAt.find(peerID);
					const auto responseIt = lastShardTransferResponseAt.find(peerID);
					const bool responseAfterProbe =
						responseIt != lastShardTransferResponseAt.end() &&
						(probeIt == lastShardTransferProbeAt.end() ||
						 responseIt->second >= probeIt->second);
					peerReady = responseAfterProbe && Interlink::Get().IsConnectedTo(peerID);
					if (!peerReady &&
						(probeIt == lastShardTransferProbeAt.end() ||
						 (now - probeIt->second) >= std::chrono::milliseconds(250)))
					{
						lastShardTransferProbeAt[peerID] = now;
						sendProbe = true;
					}
				}

				if (peerReady)
				{
					++readyPeerCount;
					continue;
				}

				allPeersReady = false;
				if (sendProbe)
				{
					Interlink::Get().SendMessage(peerID, probePacket,
												 NetworkMessageSendFlag::eReliableNow);
				}
			}

			if (!allPeersReady &&
				(lastTransferReadinessLogAt == InitClock::time_point::min() ||
				 (now - lastTransferReadinessLogAt) >= std::chrono::seconds(1)))
			{
				logger.DebugFormatted(
					"Sandbox shard bootstrap is waiting for shard transfer readiness: {}/{} peers ready.",
					readyPeerCount, liveShardPeers.size());
				lastTransferReadinessLogAt = now;
			}

			return allPeersReady;
		};

	bool initialEntitySeedHandled = false;
	while (!ShouldShutdown && !initialEntitySeedHandled)
	{
		if (!BoundLeaser::Get().HasBound())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		try
		{
			// Recovery must complete successfully before we decide whether an empty shard
			// should seed its initial sandbox entities.
			SnapshotService::Get().RecoverClaimedBoundSnapshotIfNeeded();

			if (!BoundLeaser::Get().HasBound())
			{
				logger.Warning("Sandbox shard lost its claimed bound during init; idling.");
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				continue;
			}

			const BoundsID boundID = BoundLeaser::Get().GetBoundID();
			const long long entityOwnerEntries = InternalDB::Get()->HLen("Entity:EntityOwner");
			const size_t localEntityCount = EntityLedger::Get().GetEntityCount();
			if (!BoundLeaser::Get().HasBound() || BoundLeaser::Get().GetBoundID() != boundID)
			{
				logger.Warning("Sandbox shard lost or changed its claimed bound during init; idling.");
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				continue;
			}
			if (boundID == 0 && entityOwnerEntries < kInitialSandboxEntityCount)
			{
				if (!areShardTransfersReadyForBootstrap())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				if (!TryAcquireInitialSandboxSeedLease() && !RefreshInitialSandboxSeedLeaseIfOwner())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				vec3 spawnCenter = vec3(0, 0, 0);
				bool hasSpawnCenter = false;
				BoundLeaser::Get().GetBound(
					[&](const IBounds& bound)
					{
						spawnCenter = bound.GetCenter();
						hasSpawnCenter = true;
					});
				if (!hasSpawnCenter || !BoundLeaser::Get().HasBound() ||
					BoundLeaser::Get().GetBoundID() != boundID)
				{
					logger.Warning(
						"Sandbox shard could not resolve its claimed bound center during init; retrying bootstrap.");
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				const int remainingEntityCount =
					static_cast<int>(kInitialSandboxEntityCount - entityOwnerEntries);
				size_t spawnedEntityCount = 0;
				std::mt19937 rng(std::random_device{}());
				std::uniform_real_distribution<float> dist(0.0f, 1.0f);
				for (int i = 0; i < remainingEntityCount; i++)
				{
					float z = dist(rng) * 2.0f - 1.0f;					 // z in [-1, 1]
					float theta = dist(rng) * 2.0f * glm::pi<float>();	 // angle around Z

					float r = std::sqrt(1.0f - z * z) * 120.0f;

					float x = r * std::cos(theta);
					float y = r * std::sin(theta);

					vec3 velocityVec = vec3(x, y, 0);
					AtlasTransform t;
					const float spawnRadius = dist(rng) * 5.0f;
					t.position =
						spawnCenter + vec3(std::cos(theta), std::sin(theta), 0.0f) * spawnRadius;
					ByteWriter metadataWriter;
					metadataWriter.vec3(velocityVec);
					AtlasNet_CreateEntity(t, metadataWriter.bytes());
					++spawnedEntityCount;
				}
				logger.DebugFormatted(
					"Seeded {} initial sandbox entities on bound {} after shard transfer readiness succeeded. Local entity count is now {}.",
					spawnedEntityCount, boundID,
					EntityLedger::Get().GetEntityCount());
			}
			else
			{
				logger.DebugFormatted(
					"Skipping initial sandbox entity spawn. Bound ID: {} Entity:EntityOwner entries: {} local entities: {}",
					boundID, entityOwnerEntries, localEntityCount);
			}

			initialEntitySeedHandled = true;
		}
		catch (const std::exception& ex)
		{
			logger.WarningFormatted(
				"Sandbox shard init is idling until database access recovers. {}",
				ex.what());
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
		catch (...)
		{
			logger.Warning("Sandbox shard init is idling until database access recovers.");
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	}

	using Clock = std::chrono::steady_clock;

	auto lastTime = Clock::now();
	auto fpsTimer = Clock::now();

	constexpr double TargetFPS = 60.0;
	constexpr std::chrono::duration<double> TargetFrameTime(1.0 / TargetFPS);

	constexpr float MinX = -100.f;
	constexpr float MaxX = 100.f;
	constexpr float MinY = -100.f;
	constexpr float MaxY = 100.f;

	uint64_t frameCount = 0;
	double accumulatedTime = 0.0;


	while (!ShouldShutdown)
	{
		const auto frameStart = Clock::now();

		const std::chrono::duration<double> delta = frameStart - lastTime;
		lastTime = frameStart;

		const double dt = delta.count();  // seconds
		accumulatedTime += dt;
		frameCount++;

		EntityLedger::Get().ForEachEntityWrite(
			std::execution::par_unseq,
			[&](AtlasEntity& e)
			{
				if (e.IsClient)
				{
					// GetCommandBus().Dispatch(e.Client_ID, GameStateCommand{});
					return;
				}
				vec3 velocity = ByteReader(e.payload).vec3();

				// Integrate
				e.transform.position += velocity * static_cast<float>(dt);
				vec3& pos = e.transform.position;

				// ---- X Axis ----
				if (pos.x > MaxX)
				{
					pos.x = MaxX;
					velocity.x *= -1.F;
				}
				else if (pos.x < MinX)
				{
					pos.x = MinX;
					velocity.x *= -1.F;
				}

				// ---- Y Axis ----
				if (pos.y > MaxY)
				{
					pos.y = MaxY;
					velocity.y *= -1.f;
				}
				else if (pos.y < MinY)
				{
					pos.y = MinY;
					velocity.y *= -1.f;
				}

				ByteWriter metadataWriter = ByteWriter().vec3(velocity);
				e.payload.assign(metadataWriter.bytes().begin(), metadataWriter.bytes().end());
			});

		GetCommandBus().Flush();
		// ---- FPS print every second ----
		const auto now = Clock::now();
		const std::chrono::duration<double> fpsElapsed = now - fpsTimer;

		if (fpsElapsed.count() >= 1.0)
		{
			const double avgFPS = frameCount / accumulatedTime;
			// std::cout << "Active FPS: " << avgFPS << std::endl;

			frameCount = 0;
			accumulatedTime = 0.0;
			fpsTimer = now;
		}

		// ---- Frame limiting to 60 FPS ----
		const auto frameEnd = Clock::now();
		const std::chrono::duration<double> frameTime = frameEnd - frameStart;

		if (frameTime < TargetFrameTime)
		{
			std::this_thread::sleep_for(TargetFrameTime - frameTime);
		}
	}
}

void SandboxServer::Shutdown()
{
	ShouldShutdown = true;
	IAtlasNetServer::Shutdown();
}

void SandboxServer::OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
								  AtlasEntityPayload& payload)
{
	GameStateCommand setPosCommand;
	setPosCommand.yourPosition = c.spawnLocation.position;
	GetCommandBus().Dispatch(c.client.ID, setPosCommand);
	logger.DebugFormatted("Sending a GameServerState to {} Setting initial pos to {}",
						  UUIDGen::ToString(c.client.ID), glm::to_string(c.spawnLocation.position));
}
void SandboxServer::OnGameClientInputCommand(const NetClientIntentHeader& header,
											 const GameClientInputCommand& command)
{
	/* logger.DebugFormatted("Received a GameClientInputCommand from {} requesting to move to {}",
						  UUIDGen::ToString(header.clientID),
						  glm::to_string(command.myDesiredDestination)); */
	if (!EntityLedger::Get().ExistsEntity(header.entityID))
	{
		logger.ErrorFormatted("GameClient Input command THROWN AWAY. THIS SHOULD NOT HAPPEN");
		return;
	}
	EntityLedger::Get().GetEntity(header.entityID, [&](AtlasEntity& e)
								  { e.transform.position = command.myDesiredDestination; });
	GameStateCommand response;
	response.yourPosition = command.myDesiredDestination;
	/* logger.DebugFormatted("Responding a GameServerState to {} approving move to {}",
						  UUIDGen::ToString(header.clientID),
						  glm::to_string(command.myDesiredDestination)); */

	EntityLedger::Get().ForEachEntityRead(
		std::execution::seq,
		[&](const AtlasEntity& e)
		{
			response.entities.push_back(
				GameStateCommand::Entity{.position = e.transform.position, .ID = e.Entity_ID});
		});
	GetCommandBus().Dispatch(header.clientID, response);
}

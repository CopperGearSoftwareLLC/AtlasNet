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
#include "Entity/Transform.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Events/GlobalEvents.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Snapshot/SnapshotService.hpp"

namespace
{
constexpr int kInitialSandboxEntityCount = 100;
constexpr std::string_view kInitialSandboxSeedCounterKey = "Sandbox:InitialEntitySeedCounter";

bool TryClaimInitialSandboxSeedCounter()
{
	const auto response = InternalDB::Get()->WithSync(
		[&](auto& r)
		{
			std::array<std::string, 3> cmd = {"GETSET",
											  std::string(kInitialSandboxSeedCounterKey), "1"};
			return r.command(cmd.begin(), cmd.end());
		});

	return !response || response->type == REDIS_REPLY_NIL ||
		   (response->type == REDIS_REPLY_STRING && response->str != nullptr &&
			std::string_view(response->str, static_cast<size_t>(response->len)) == "0") ||
		   (response->type == REDIS_REPLY_INTEGER && response->integer == 0);
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
			const size_t localEntityCount = EntityLedger::Get().GetEntityCount();
			if (!BoundLeaser::Get().HasBound() || BoundLeaser::Get().GetBoundID() != boundID)
			{
				logger.Warning("Sandbox shard lost or changed its claimed bound during init; idling.");
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				continue;
			}
			if (boundID == 0 && localEntityCount == 0)
			{
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

				if (!TryClaimInitialSandboxSeedCounter())
				{
					logger.DebugFormatted(
						"Skipping initial sandbox entity spawn. Bound {} did not win the atomic seed claim.",
						boundID);
					initialEntitySeedHandled = true;
					continue;
				}

				size_t spawnedEntityCount = 0;
				std::mt19937 rng(std::random_device{}());
				std::uniform_real_distribution<float> dist(0.0f, 1.0f);
				for (int i = 0; i < kInitialSandboxEntityCount; i++)
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
					"Seeded {} initial sandbox entities on bound {} after winning the atomic seed claim. Local entity count is now {}.",
					spawnedEntityCount, boundID,
					EntityLedger::Get().GetEntityCount());
			}
			else
			{
				logger.DebugFormatted(
					"Skipping initial sandbox entity spawn. Bound ID: {} local entities: {}",
					boundID, localEntityCount);
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

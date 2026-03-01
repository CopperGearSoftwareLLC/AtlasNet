#include "SandboxServer.hpp"

#include <chrono>
#include <execution>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Command/NetCommand.hpp"
#include "Commands/GameClientInputCommand.hpp"
#include "Commands/GameStateCommand.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/BoundLeaser.hpp"

void SandboxServer::Run()
{
	AtlasNet_Initialize();

	GetCommandBus().Subscribe<GameClientInputCommand>(
		[this](const NetClientIntentHeader& h, const GameClientInputCommand& c)
		{ OnGameClientInputCommand(h, c); });

	EventSystem::Get().Subscribe<ClientConnectEvent>(
		[&](const ClientConnectEvent& e)
		{
			logger.DebugFormatted("Client Connected event!\n - ID: {}\n - IP: {}\n - Proxy: {}",
								  UUIDGen::ToString(e.client.ID), e.client.ip.ToString(),
								  e.ConnectedProxy.ToString());
		});

	while (!BoundLeaser::Get().HasBound())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	if (const auto& Bound = BoundLeaser::Get().GetBound(); Bound.ID == 0)
	{
		std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		for (int i = 0; i < 100; i++)
		{
			float z = dist(rng) * 2.0f - 1.0f;					// z in [-1, 1]
			float theta = dist(rng) * 2.0f * glm::pi<float>();	// angle around Z

			float r = std::sqrt(1.0f - z * z) * 120.0f;

			float x = r * std::cos(theta);
			float y = r * std::sin(theta);

			vec3 velocityVec = vec3(x, y, 0);
			Transform t;
			t.position = vec3(0, 0, 0);
			ByteWriter metadataWriter;
			metadataWriter.vec3(velocityVec);
			AtlasEntityHandle e = AtlasNet_CreateEntity(t, metadataWriter.bytes());
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

	AtlasEntityHandle h;

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
					GetCommandBus().Dispatch(e.Client_ID, GameStateCommand{});
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
					velocity.x *= -1.f;
				}
				else if (pos.x < MinX)
				{
					pos.x = MinX;
					velocity.x *= -1.f;
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
void SandboxServer::OnClientSpawn(const ClientSpawnInfo& c)
{
	logger.DebugFormatted("SPAWNING CLIENT ENTITY FOR CLIENT {}", UUIDGen::ToString(c.client.ID));
	AtlasEntityHandle clientHandle = AtlasNet_CreateClientEntity(c.client.ID, c.spawnLocation);
}
void SandboxServer::OnGameClientInputCommand(const NetClientIntentHeader& header,
											 const GameClientInputCommand& command)
{
	logger.Debug("Received a GameClientInputCommand");
}

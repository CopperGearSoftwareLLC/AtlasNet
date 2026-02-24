#include "SandboxServer.hpp"

#include <chrono>
#include <execution>
#include <thread>

#include "AtlasNetServer.hpp"
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
	AtlasNetServer::InitializeProperties InitProperties;
	InitProperties.OnShutdownRequest = [&](SignalType signal) { ShouldShutdown = true; };
	AtlasNetServer::Get().Initialize(InitProperties);

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
		for (int i = 0; i < 4; i++)
		{
			float z = dist(rng) * 2.0f - 1.0f;					// z in [-1, 1]
			float theta = dist(rng) * 2.0f * glm::pi<float>();	// angle around Z

			float r = std::sqrt(1.0f - z * z) * 120.0f;

			float x = r * std::cos(theta);
			float y = r * std::sin(theta);

			vec3 velocityVec = vec3(x, y, 0);
			Transform t;
			t.position = vec3(0,0, 0);
			ByteWriter metadataWriter;
			metadataWriter.vec3(velocityVec);
			AtlasEntityHandle e = AtlasNetServer::Get().CreateEntity(t, metadataWriter.bytes());
		}
	}

	using Clock = std::chrono::steady_clock;

	auto lastTime = Clock::now();
	constexpr float MinX = -100.f;
	constexpr float MaxX = 100.f;
	constexpr float MinY = -100.f;
	constexpr float MaxY = 100.f;
	while (!ShouldShutdown)
	{
		const auto now = Clock::now();
		const std::chrono::duration<double> delta = now - lastTime;
		lastTime = now;

		const float dt = delta.count();	 // seconds as double

		EntityLedger::Get().ForEachEntity(
			std::execution::par_unseq,
			[&](AtlasEntity& e)
			{
				vec3 velocity = ByteReader(e.Metadata).vec3();

				// Integrate
				e.data.transform.position += velocity * dt;

				vec3& pos = e.data.transform.position;

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

				// Write velocity back if needed
				ByteWriter metadataWriter = ByteWriter().vec3(velocity);
				e.Metadata.assign(metadataWriter.bytes().begin(), metadataWriter.bytes().end());
			});
	}
}

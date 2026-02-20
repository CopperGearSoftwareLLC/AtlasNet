#include "SandboxServer.hpp"

#include <chrono>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Global/Misc/UUID.hpp"

void SandboxServer::Run()
{
	AtlasNetServer::InitializeProperties InitProperties;
	InitProperties.OnShutdownRequest = [&](SignalType signal)
	{ ShouldShutdown = true; };
	AtlasNetServer::Get().Initialize(InitProperties);

	EventSystem::Get().Subscribe<ClientConnectEvent>(
		[&](const ClientConnectEvent& e)
		{
			logger.DebugFormatted(
				"Client Connected event!\n - ID: {}\n - IP: {}\n - Proxy: {}",
				UUIDGen::ToString(e.client.ID), e.client.ip.ToString(),
				e.ConnectedProxy.ToString());
		});
	while (!ShouldShutdown)
	{
		std::span<AtlasEntity> myspan;
		std::vector<AtlasEntity> Incoming;
		std::vector<AtlasEntityID> Outgoing;
		AtlasNetServer::Get().Update(myspan, Incoming, Outgoing);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

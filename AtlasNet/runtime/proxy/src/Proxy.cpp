#include "Proxy.hpp"

#include <thread>

#include "Debug/Crash/CrashHandler.hpp"
#include "Events/EventSystem.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
void Proxy::Run()
{
	logger->Debug("Init");	// hello

	Init();
	logger->Debug("Loop Entry");
	while (!ShouldShutdown)
	{
		// Interlink::Get().Tick();
		// ProxyLink::Get().Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	logger->Debug("Begin shutdown");
	CleanUp();
	logger->Debug("Shutdown");
}
void Proxy::Init()
{
	CrashHandler::Get().Init();
	NetworkCredentials::Make(NetworkIdentity(NetworkIdentityType::eProxy, UUIDGen::Gen()));

	Interlink::Get().Init();
	NetworkManifest::Get().ScheduleNetworkPings();
	HealthManifest::Get().ScheduleHealthPings();
	EventSystem::Get().Init();
}
void Proxy::Shutdown()
{
	ShouldShutdown = true;
}
void Proxy::CleanUp()
{
	// Interlink::Get().Shutdown();
}
bool Proxy::OnAcceptConnection(const Connection& c)
{
	return true;
}
void Proxy::OnConnected(const NetworkIdentity& id) {}
void Proxy::OnDisconnected(const NetworkIdentity& id) {}
void Proxy::OnMessageReceived(const Connection& from, std::span<const std::byte> data) {}

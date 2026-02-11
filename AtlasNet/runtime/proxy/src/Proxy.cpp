#include "Proxy.hpp"

#include <thread>

#include "Crash/CrashHandler.hpp"
#include "Database/HealthManifest.hpp"
#include "Interlink.hpp"
#include "Misc/UUID.hpp"
#include "Telemetry/NetworkManifest.hpp"
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
	ID = NetworkIdentity(NetworkIdentityType::eProxy, UUIDGen::Gen());
	Interlink::Get().Init(InterlinkProperties{.ThisID = ID, .logger = logger});
	NetworkManifest::Get().ScheduleNetworkPings(ID);
	HealthManifest::Get().ScheduleHealthPings(ID);
	
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
void Proxy::OnMessageReceived(const Connection& from,
							  std::span<const std::byte> data)
{
}

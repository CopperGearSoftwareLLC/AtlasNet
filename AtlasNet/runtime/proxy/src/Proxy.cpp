#include "Proxy.hpp"

#include <thread>

#include "Crash/CrashHandler.hpp"
#include "Database/HealthManifest.hpp"
#include "Interlink.hpp"
#include "InterlinkIdentifier.hpp"
void Proxy::Run()
{
	logger->Debug("Init");

	Init();
	logger->Debug("Loop Entry");
	while (!ShouldShutdown)
	{
		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	logger->Debug("Begin shutdown");
	CleanUp();
	logger->Debug("Shutdown");
}
void Proxy::Init()
{
	CrashHandler::Get().Init();
	ID = InterLinkIdentifier(InterlinkType::eProxy, DockerIO::Get().GetSelfContainerName());
	HealthManifest::Get().ScheduleHealthPings(ID);

	Interlink::Get().Init(InterlinkProperties{
		.ThisID = ID,
		.logger = logger,
		.callbacks = {
			.acceptConnectionCallback = [this](const Connection& c)
			{ return OnAcceptConnection(c); },
			.OnConnectedCallback = [this](const InterLinkIdentifier& id) { OnConnected(id); },
			//.OnMessageArrival = [this](const Connection& from, std::span<const std::byte> data)
			//{ OnMessageReceived(from, data); },
			.OnDisconnectedCallback = [this](const InterLinkIdentifier& id)
			{ OnDisconnected(id); }}});
}
void Proxy::Shutdown()
{
	ShouldShutdown = true;
}
void Proxy::CleanUp()
{
	Interlink::Get().Shutdown();
}
bool Proxy::OnAcceptConnection(const Connection& c)
{
	return true;
}
void Proxy::OnConnected(const InterLinkIdentifier& id) {}
void Proxy::OnDisconnected(const InterLinkIdentifier& id) {}
void Proxy::OnMessageReceived(const Connection& from, std::span<const std::byte> data) {}

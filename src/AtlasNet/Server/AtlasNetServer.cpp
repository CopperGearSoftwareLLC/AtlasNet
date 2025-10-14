#include "AtlasNetServer.hpp"

#include "Interlink/Interlink.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties &properties)
{
	CrashHandler::Get().Init(properties.ExePath);
	DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});
	InterLinkIdentifier myID = InterLinkIdentifier(InterlinkType::eGameServer, DockerIO::Get().GetSelfContainerName());
	logger = std::make_shared<Log>(myID.ToString());
	logger->Debug("AtlasNet Initialize");
	Interlink::Check();
	Interlink::Get().Init(
		InterlinkProperties{.ThisID = myID,
							.logger = logger,
							.callbacks = {.acceptConnectionCallback = [](const Connection &c)
										  { return true; },
										  .OnConnectedCallback = nullptr}});
}

void AtlasNetServer::Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
							std::vector<AtlasEntityID> &OutgoingEntities)
{
	Interlink::Get().Tick();
}

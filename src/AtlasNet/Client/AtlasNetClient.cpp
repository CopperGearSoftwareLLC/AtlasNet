#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"

void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
    CrashHandler::Get().Init(props.ExePath);

    std::string serverName = props.ServerName;
    if (serverName.empty())
        serverName = DockerIO::Get().GetSelfContainerName();

    InterLinkIdentifier myID(InterlinkType::eGameClient, serverName);
    logger = std::make_shared<Log>(myID.ToString());
    logger->Debug("AtlasNetClient Initialize");
    logger->DebugFormatted("Client ID: {}", myID.ID);
    logger->DebugFormatted("Client identifier: {}", myID.ToString());

    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
    {
      logger->Error(std::string("GameNetworkingSockets_Init failed: ") + errMsg);
      return;
    }
    else
    {
      logger->Debug("GameNetworkingSockets_Init succeeded");
      system("ping www.google.com"); // keep for debug purposes
    }

    logger->Debug("Waiting for Interlink auto-connect to GameServer...");
}

void AtlasNetClient::SendEntityUpdate(const AtlasEntity &entity)
{
    Interlink::Get().Tick();
    Interlink::Get().SendMessageRaw(serverID, std::as_bytes(std::span(&entity, 1)));
}

int AtlasNetClient::GetRemoteEntities(AtlasEntity *buffer, int maxCount)
{
    std::lock_guard lock(Mutex);
    int count = std::min<int>(maxCount, RemoteEntities.size());
    int i = 0;
    for (auto &[id, e] : RemoteEntities)
    {
        if (i >= count) break;
        buffer[i++] = e;
    }
    return count;
}

void AtlasNetClient::Shutdown()
{
    Interlink::Get().Shutdown();
    logger.reset();
}

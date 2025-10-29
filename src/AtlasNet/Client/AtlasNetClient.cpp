#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"

void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
    logger->Debug("[AtlasNetClient] Initialize");
    CrashHandler::Get().Init(props.ExePath);
    InterLinkIdentifier myID(InterlinkType::eGameClient, props.ClientName);
    InterLinkIdentifier God =  InterLinkIdentifier::MakeIDGod();
    logger->Debug("[AtlasNetClient] Made GodID");
    IPAddress GodIP;
    GodIP.SetIPv4(127,0,0,1,50866);
    logger->Debug("[AtlasNetClient] Set God IPv4");
    Interlink::Get().EstablishConnectionAtIP(God, GodIP);
    logger->Debug("[AtlasNetClient] establishing connection to God");
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

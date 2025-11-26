#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"
#include "misc/UUID.hpp"
void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
  logger->Debug("[AtlasNetClient] Initialize");
  InterLinkIdentifier myID(InterlinkType::eGameClient, UUIDGen::encode_base20(UUIDGen::Gen()));
  InterLinkIdentifier God =  InterLinkIdentifier::MakeIDGod();
  serverID = God;
  logger->Debug("[AtlasNetClient] Made my & GodID");
  IPAddress GodIP;
  GodIP.SetIPv4(127,0,0,1,_PORT_GOD);
  logger->Debug("[AtlasNetClient] Set God IPv4");
  
  Interlink::Get().Init(
  InterlinkProperties{
    .ThisID = myID,
    .logger = logger,
    .callbacks = {.acceptConnectionCallback = [](const Connection &c)
            { return true; },
            .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
            .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {}}});
  
    Interlink::Get().EstablishConnectionAtIP(God, GodIP);
    logger->Debug("[AtlasNetClient] establishing connection to God");
}

void AtlasNetClient::SendEntityUpdate(const AtlasEntity &entity)
{
    Interlink::Get().SendMessageRaw(serverID, std::as_bytes(std::span(&entity, 1)));
}
void AtlasNetClient::Tick()
{
    Interlink::Get().Tick();
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

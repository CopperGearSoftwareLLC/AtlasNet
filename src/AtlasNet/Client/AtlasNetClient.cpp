#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"
#include "misc/UUID.hpp"
void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
  logger->Debug("[AtlasNetClient] Initialize");
  InterLinkIdentifier ID(InterlinkType::eGameClient, UUIDGen::encode_base20(UUIDGen::Gen()));
  myID = ID;
  //InterLinkIdentifier God =  InterLinkIdentifier::MakeIDGod();
  GameCoordinatorID = InterLinkIdentifier::MakeIDGameCoordinator();
  logger->Debug("[AtlasNetClient] Made my & GCID");
  //IPAddress GodIP;
  //GodIP.SetIPv4(127,0,0,1,_PORT_GOD);
  IPAddress GameCoordinatorIP;
  GameCoordinatorIP.SetIPv4(127,0,0,1,_PORT_GAMECOORDINATOR);
  logger->Debug("[AtlasNetClient] Set GC IPv4");

  Interlink::Get().Init(
  InterlinkProperties{
    .ThisID = myID,
    .logger = logger,
    .callbacks = {.acceptConnectionCallback = [](const Connection &c) { return true; },
            .OnConnectedCallback = [this](const InterLinkIdentifier &Connection) 
            {
              OnConnected(Connection);
            },
            .OnMessageArrival = [this](const Connection &fromWhom, std::span<const std::byte> data) 
            {
              OnMessageReceived(fromWhom, data); 
            }
          }});
  
    Interlink::Get().EstablishConnectionAtIP(GameCoordinatorID, GameCoordinatorIP);
    logger->Debug("[AtlasNetClient] establishing connection to Game Coordinator");
}

void AtlasNetClient::SendEntityUpdate(const AtlasEntity &entity)
{
    if (!proxyID.ID.empty())
        Interlink::Get().SendMessageRaw(proxyID, std::as_bytes(std::span(&entity, 1)));
}
void AtlasNetClient::Tick()
{
    Interlink::Get().Tick();
}
int AtlasNetClient::GetRemoteEntities(AtlasEntity *buffer, int maxCount)
{
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
}

void AtlasNetClient::OnConnected(const InterLinkIdentifier &identifier)
{
    // Accept all incoming connections
    logger->DebugFormatted("[AtlasNetClient] connected to {}", identifier.ToString());
    if (identifier.Type == InterlinkType::eDemigod)
    {
        // likely changing proxies, not game coordinator rerouting us
        if (IsConnectedToProxy == true)
            return;
        
        // stop the connection
        IsConnectedToProxy = true;
        Interlink::Get().SendMessageRaw(GameCoordinatorID, std::as_bytes(std::span("ProxyConnected")));
        logger->Debug("[AtlasNetClient] Messaged Game Coordinator of proxy connection success.");
    }
    if (identifier.Type == InterlinkType::eGameCoordinator)
    {
        logger->DebugFormatted("[AtlasNetClient] connected GameCoordinator ID {}", GameCoordinatorID.ToString());
    }
}

void AtlasNetClient::OnMessageReceived(const Connection& from, std::span<const std::byte> data)
{
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[AtlasNetClient] Message from {}: {}",
                           from.target.ToString(), msg);

    // Only care about GameClient confirmations for now
    if (from.target.Type == InterlinkType::eGameCoordinator)
    {
        // The message is expected to be only "IP:Port"
        std::string address = msg;

        // Find the colon separating IP and port
        size_t colonPos = address.find(':');
        if (colonPos == std::string::npos)
        {
            logger->Error("[AtlasNetClient] Invalid proxy address (missing ':' separator).");
            return;
        }

        // Extract IP and port substrings
        std::string ipStr = address.substr(0, colonPos);
        std::string portStr = address.substr(colonPos + 1);

        logger->DebugFormatted("[AtlasNetClient] Proxy Address {}:{}",
                           ipStr, portStr);

        // Convert port to integer
        PortType port = static_cast<PortType>(std::stoi(portStr));

        // Fill IPAddress
        IPAddress ProxyIP;
        ProxyIP.Parse(address);

        // Build the identifier for this proxy
        proxyID.Type = InterlinkType::eDemigod;
        proxyID.ID   = "DemigodProxy";  // update this if needed

        logger->DebugFormatted("[AtlasNetClient] Connecting to proxy {} at {}:{}",
                               proxyID.ToString(), ipStr, port);

        // Establish connection
        Interlink::Get().EstablishConnectionAtIP(proxyID, ProxyIP);
    }
    else if (from.target.Type == InterlinkType::eDemigod)
    {
        HandleEntityMessage(data);
    }
}

void AtlasNetClient::HandleEntityMessage(const std::span<const std::byte>& data)
{
    if (data.size() < 1)
    {
        logger->Error("[AtlasNetClient] Received empty message ABORT");
        return;
    }

    const AtlasNetMessageHeader header = static_cast<AtlasNetMessageHeader>(data[0]);
    const std::byte *payload = data.data() + 1;
    const size_t payloadSize = data.size() - 1;

    if (payloadSize % sizeof(AtlasEntity) != 0)
    {
        logger->ErrorFormatted("[AtlasNetClient] Invalid payload size {}, ABORT", payloadSize);
        return;
    }

    const size_t entityCount = payloadSize / sizeof(AtlasEntity);
    std::vector<AtlasEntity> entities(entityCount);
    std::memcpy(entities.data(), payload, payloadSize);
    {
        std::lock_guard<std::mutex> lock(Mutex);
        
        switch (header)
        {
            case AtlasNetMessageHeader::EntityUpdate:
            {
                for (const auto &entity : entities)
                    RemoteEntities[entity.ID] = entity;
    
                logger->DebugFormatted("[AtlasNetClient] EntityUpdate: {} entities", entities.size());
                break;
            }
            case AtlasNetMessageHeader::EntityIncoming:
            {
                for (const auto &entity : entities)
                {
                    RemoteEntities[entity.ID] = entity;
                }
                logger->DebugFormatted("[AtlasNetClient] Cached EntityIncoming: {} entities", entities.size());
                break;
            }
            case AtlasNetMessageHeader::EntityOutgoing:
            {
                for (const auto &entity : entities)
                {
                    RemoteEntities.erase(entity.ID);
                }
                logger->DebugFormatted("[AtlasNetClient] Cached EntityOutgoing: {} entities", entities.size());
                break;
            }
    
            default:
                logger->ErrorFormatted("[AtlasNetClient] Unknown AtlasNetMessageHeader {}",
                                       static_cast<int>(header));
                return;
        }
    }

}

void AtlasNetClient::Shutdown()
{
    Interlink::Get().Shutdown();
    logger.reset();
}

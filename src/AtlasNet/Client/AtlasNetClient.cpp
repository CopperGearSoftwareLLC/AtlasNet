#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"
#include "misc/UUID.hpp"
void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
  logger->Debug("[AtlasNetClient] Initialize");
  InterLinkIdentifier myID(InterlinkType::eGameClient, UUIDGen::encode_base20(UUIDGen::Gen()));
  //InterLinkIdentifier God =  InterLinkIdentifier::MakeIDGod();
  InterLinkIdentifier GC = InterLinkIdentifier::MakeIDGameCoordinator();
  serverID = GC;
  logger->Debug("[AtlasNetClient] Made my & GCID");
  IPAddress GCIP;
  //GodIP.SetIPv4(127,0,0,1,_PORT_GOD);
  GCIP.SetIPv4(127,0,0,1,_PORT_GAMECOORDINATOR);
  logger->Debug("[AtlasNetClient] Set GC IPv4");
  
  Interlink::Get().Init(
  InterlinkProperties{
    .ThisID = myID,
    .logger = logger,
    .callbacks = {.acceptConnectionCallback = [](const Connection &c)
            { return true; },
            .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
            .OnMessageArrival = [this](const Connection &fromWhom, std::span<const std::byte> data) 
            {
              OnMessageReceived(fromWhom, data); 
            }
          }});
  
    Interlink::Get().EstablishConnectionAtIP(GC, GCIP);
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
        //ProxyIP.Parse(address);
        //ProxyIP.SetIPv4(127,0,0,1, port);
        ProxyIP.SetIPv4(127,0,0,1,30000);

        // Build the identifier for this proxy
        InterLinkIdentifier ProxyID;
        ProxyID.Type = InterlinkType::eDemigod;
        ProxyID.ID   = "DemigodProxy";  // update this if needed

        logger->DebugFormatted("[AtlasNetClient] Connecting to proxy {} at {}:{}",
                               ProxyID.ToString(), ipStr, port);

        // Establish connection
        Interlink::Get().EstablishConnectionAtIP(ProxyID, ProxyIP);
    }
}

void AtlasNetClient::Shutdown()
{
    Interlink::Get().Shutdown();
    logger.reset();
}

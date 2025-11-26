#include "Demigod.hpp"
#include "Interlink/InterlinkIdentifier.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"

Demigod::Demigod() {}
Demigod::~Demigod() { Shutdown(); }

void Demigod::Init()
{
    // 1. Build our identifier based on Docker container name
    InterLinkIdentifier demigodIdentifier(
        InterlinkType::eDemigod,
        DockerIO::Get().GetSelfContainerName());

    logger = std::make_shared<Log>(demigodIdentifier.ToString());

    logger->DebugFormatted("[Demigod] Initializing as {}", demigodIdentifier.ToString());

    // 2. Initialize Interlink
    Interlink::Get().Init(
        InterlinkProperties{
            .ThisID = demigodIdentifier,
            .logger = logger,
            .callbacks = {
                .acceptConnectionCallback = [this](const Connection& c)
                {
                    return OnAcceptConnection(c);
                },
                .OnConnectedCallback = [this](const InterLinkIdentifier& id)
                {
                    OnConnected(id);
                },
                .OnMessageArrival = [this](const Connection& from, std::span<const std::byte> data)
                {
                    OnMessageReceived(from, data);
                }
            }
        });

    // 3. Register in ProxyRegistry with internal IP + port
    //uint16_t listenPort = Type2ListenPort.at(InterlinkType::eDemigod);
    //IPAddress ip;
    //ip.Parse(DockerIO::Get().GetSelfContainerIP() + ":" + std::to_string(listenPort));
    //ProxyRegistry::Get().RegisterSelf(demigodIdentifier, ip);

    SelfID = demigodIdentifier;

    logger->Debug("[Demigod] Interlink initialized and registered in ProxyRegistry.");

    // Optionally: pre-discover partitions from ServerRegistry
    std::unordered_map<InterLinkIdentifier, ServerRegistryEntry> serverMap =
        ServerRegistry::Get().GetServers();

    for (auto& [id, entry] : serverMap)
    {
        if (id.Type == InterlinkType::ePartition)
        {
            logger->DebugFormatted("[Demigod] {}", id.ID);
            partitions.emplace(id);
        }
    }
}


void Demigod::Run()
{
    logger->Debug("[Demigod] Running main loop...");
    while (!ShouldShutdown)
    {
        Interlink::Get().Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    logger->Debug("[Demigod] Main loop exited.");
}

void Demigod::Shutdown()
{
    ShouldShutdown = true;
    Interlink::Get().Shutdown();
    logger->Debug("[Demigod] Shutdown complete.");
}

bool Demigod::OnAcceptConnection(const Connection& c)
{
    // Accept all incoming connections
    logger->DebugFormatted("[Demigod] Accepting connection from {}", c.address.ToString());
    ProxyRegistry::Get().IncrementClient(SelfID);
    return true;
}

void Demigod::OnConnected(const InterLinkIdentifier& id)
{
    logger->DebugFormatted("[Demigod] Connection established with {}",
                           id.ToString());

    if (id.Type == InterlinkType::ePartition)
    {
        partitions.emplace(id);
        logger->DebugFormatted("[Demigod] Registered partition {}", id.ToString());
    }
    else if (id.Type == InterlinkType::eGameClient)
    {
        const std::string clientKey = id.ToString();
        connectedClients.insert(clientKey);

        // Note: load increment is already done in ProxyRegistry::AssignClientToProxy
        // called by GameCoordinator. If you want per-connection tracking here, you can:
        // ProxyRegistry::Get().IncrementClient(SelfID);

        logger->DebugFormatted("[Demigod] Client {} connected", clientKey);
    }
}

void Demigod::OnMessageReceived(const Connection& from,
                                std::span<const std::byte> data)
{
    // For now, treat as text for control / debug messages.
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[Demigod] Message from {} (type={}): {}",
                           from.target.ToString(),
                           static_cast<int>(from.target.Type),
                           msg);

    // Control messages (e.g., from GOD)
    if (msg.rfind("AuthorityChange:", 0) == 0)
    {
        HandleControlMessage(from, msg);
        return;
    }

    // Route data either from clients to partitions or from partitions → clients.
    if (from.target.Type == InterlinkType::eGameClient)
    {
        ForwardClientToPartition(from, data);
    }
    else if (from.target.Type == InterlinkType::ePartition)
    {
        ForwardPartitionToClient(from, data);
    }
    else
    {
        logger->WarningFormatted("[Demigod] Unknown message origin type {}",
                                 static_cast<int>(from.target.Type));
    }
}

void Demigod::HandleControlMessage(const Connection& /*from*/,
                                   const std::string& msg)
{
    // Expected format: "AuthorityChange:ClientID PartitionID"
    const std::string prefix = "AuthorityChange:";
    if (!msg.starts_with(prefix))
        return;

    std::string rest = msg.substr(prefix.size());
    // rest = "ClientID PartitionString"
    auto spacePos = rest.find(' ');
    if (spacePos == std::string::npos)
    {
        logger->ErrorFormatted("[Demigod] Invalid AuthorityChange message: {}", msg);
        return;
    }

    std::string clientID   = rest.substr(0, spacePos);
    std::string partitionS = rest.substr(spacePos + 1);

    auto maybePartition = InterLinkIdentifier::FromString(partitionS);
    if (!maybePartition.has_value())
    {
        logger->ErrorFormatted("[Demigod] Unable to parse partition ID '{}' in AuthorityChange",
                               partitionS);
        return;
    }

    clientToPartitionMap[clientID] = maybePartition.value();
    logger->DebugFormatted("[Demigod] Updated authority: {} → {}",
                           clientID, partitionS);

}

void Demigod::ForwardClientToPartition(const Connection& from,
                                       std::span<const std::byte> data)
{
    const std::string clientKey = from.target.ToString();

    auto it = clientToPartitionMap.find(clientKey);
    if (it == clientToPartitionMap.end())
    {
        // No known partition; fallback to a simple heuristic (first partition).
        if (partitions.empty())
        {
            logger->ErrorFormatted("[Demigod] No partitions available to route client {}",
                                   clientKey);
            return;
        }

        const auto& fallbackPartition = *partitions.begin();
        clientToPartitionMap[clientKey] = fallbackPartition;

        logger->WarningFormatted("[Demigod] No authority mapping for client {}; "
                                 "assigning fallback partition {}",
                                 clientKey, fallbackPartition.ToString());
        Interlink::Get().SendMessageRaw(fallbackPartition, data,
                                        InterlinkMessageSendFlag::eReliableNow);
        return;
    }

    const InterLinkIdentifier& partitionID = it->second;
    Interlink::Get().SendMessageRaw(partitionID, data,
                                    InterlinkMessageSendFlag::eReliableNow);
    logger->DebugFormatted("[Demigod] Forwarded {} bytes from client {} to partition {}",
                           data.size(), clientKey, partitionID.ToString());
}

void Demigod::ForwardPartitionToClient(const Connection& from,
                                       std::span<const std::byte> data)
{
    const InterLinkIdentifier& partitionID = from.target;

    for (auto& [clientKey, mappedPartition] : clientToPartitionMap)
    {
        if (mappedPartition == partitionID)
        {
            auto maybeClient = InterLinkIdentifier::FromString(clientKey);
            if (!maybeClient.has_value())
            {
                logger->ErrorFormatted("[Demigod] Failed to parse client identifier '{}'",
                                       clientKey);
                continue;
            }

            Interlink::Get().SendMessageRaw(maybeClient.value(), data,
                                            InterlinkMessageSendFlag::eReliableNow);
            logger->DebugFormatted("[Demigod] Forwarded {} bytes from partition {} to client {}",
                                   data.size(), partitionID.ToString(), clientKey);
        }
    }
}
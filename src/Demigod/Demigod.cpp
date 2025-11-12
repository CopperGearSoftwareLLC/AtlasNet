#include "Demigod.hpp"
#include "Interlink/InterlinkIdentifier.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"

Demigod::Demigod() {}
Demigod::~Demigod() { Shutdown(); }

void Demigod::Init()
{
    // 1. Build our identifier based on Docker container name
    InterLinkIdentifier demigodIdentifier(InterlinkType::eDemigod, DockerIO::Get().GetSelfContainerName());
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

    logger->Debug("[Demigod] Interlink initialized successfully.");
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
    return true;
}

void Demigod::OnConnected(const InterLinkIdentifier& id)
{
    logger->DebugFormatted("[Demigod] Connection established with {}", id.ToString());

    // You could later dynamically assign clients to specific servers here
    if (id.Type == InterlinkType::eGameClient)
    {
        // Assign to a default GameServer (for now)
        auto target = InterLinkIdentifier::MakeIDGameServer("main");
        clientToServerMap[id.ToString()] = target;
        logger->DebugFormatted("[Demigod] Assigned {} to server {}", id.ToString(), target.ToString());
    }
}

void Demigod::OnMessageReceived(const Connection& from, std::span<const std::byte> data)
{
    // Convert message to string for logging
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[Demigod] Message from {} ({}): {}",
                           from.target.ToString(),
                           boost::describe::enum_to_string(from.target.Type, "Unknown"),
                           msg);

    if (from.target.Type == InterlinkType::eGameClient)
    {
        ForwardClientToServer(from, data);
    }
    else if (from.target.Type == InterlinkType::eGameServer)
    {
        ForwardServerToClient(from, data);
    }
    else
    {
        logger->WarningFormatted("[Demigod] Unknown message origin type {}", (int)from.target.Type);
    }
}

void Demigod::ForwardClientToServer(const Connection& from, std::span<const std::byte> data)
{
    auto it = clientToServerMap.find(from.target.ToString());
    if (it == clientToServerMap.end())
    {
        logger->ErrorFormatted("[Demigod] No assigned server for client {}", from.target.ToString());
        return;
    }

    const InterLinkIdentifier& serverId = it->second;
    Interlink::Get().SendMessageRaw(serverId, data, InterlinkMessageSendFlag::eReliableNow);
    logger->DebugFormatted("[Demigod] Forwarded {} bytes from client {} to server {}",
                           data.size(), from.target.ToString(), serverId.ToString());
}

void Demigod::ForwardServerToClient(const Connection& from, std::span<const std::byte> data)
{
    for (auto& [clientStr, serverId] : clientToServerMap)
    {
        if (serverId == from.target)
        {
            auto maybeClient = InterLinkIdentifier::FromString(clientStr);
            if (!maybeClient.has_value())
            {
                logger->ErrorFormatted("[Demigod] Failed to parse client identifier '{}'", clientStr);
                continue;
            }

            Interlink::Get().SendMessageRaw(maybeClient.value(), data, InterlinkMessageSendFlag::eReliableNow);
            logger->DebugFormatted("[Demigod] Forwarded {} bytes from server {} to client {}",
                                   data.size(), from.target.ToString(), clientStr);
        }
    }
}

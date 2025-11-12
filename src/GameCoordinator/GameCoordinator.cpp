#include "GameCoordinator.hpp"
#include "Docker/DockerIO.hpp"
#include "Database/ServerRegistry.hpp"
#include "Interlink/InterlinkIdentifier.hpp"

GameCoordinator::GameCoordinator() {}
GameCoordinator::~GameCoordinator() { Shutdown(); }

void GameCoordinator::Init()
{
    // 1. Identify as the GameCoordinator container
    InterLinkIdentifier coordinatorIdentifier(InterlinkType::eGameCoordinator, DockerIO::Get().GetSelfContainerName());
    logger = std::make_shared<Log>(coordinatorIdentifier.ToString());

    logger->DebugFormatted("[Coordinator] Initializing as {}", coordinatorIdentifier.ToString());

    // 2. Initialize Interlink
    Interlink::Get().Init(
        InterlinkProperties{
            .ThisID = coordinatorIdentifier,
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

    logger->Debug("[Coordinator] Interlink initialized successfully.");
}

void GameCoordinator::Run()
{
    logger->Debug("[Coordinator] Running main loop...");
    while (!ShouldShutdown)
    {
        Interlink::Get().Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    logger->Debug("[Coordinator] Main loop exited.");
}

void GameCoordinator::Shutdown()
{
    ShouldShutdown = true;
    Interlink::Get().Shutdown();
    logger->Debug("[Coordinator] Shutdown complete.");
}

bool GameCoordinator::OnAcceptConnection(const Connection& c)
{
    logger->DebugFormatted("[Coordinator] Accepting connection from {}", c.address.ToString());
    return true;
}

void GameCoordinator::OnConnected(const InterLinkIdentifier& id)
{
    logger->DebugFormatted("[Coordinator] Connection established with {}", id.ToString());

    if (id.Type == InterlinkType::eGameClient)
    {
        AssignClientToDemigod(Connection{ .target = id });
    }
}

void GameCoordinator::OnMessageReceived(const Connection& from, std::span<const std::byte> data)
{
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[Coordinator] Message from {}: {}", from.target.ToString(), msg);

    if (from.target.Type == InterlinkType::eGameClient)
        HandleClientMessage(from, msg);
}

void GameCoordinator::HandleClientMessage(const Connection& from, const std::string& msg)
{
    // Simple forward to assigned Demigod
    auto it = clientToDemigodMap.find(from.target.ToString());
    if (it == clientToDemigodMap.end())
    {
        logger->WarningFormatted("[Coordinator] No Demigod assigned for client {}, assigning now...", from.target.ToString());
        AssignClientToDemigod(from);
    }

    const auto& demigod = clientToDemigodMap[from.target.ToString()];
    Interlink::Get().SendMessageRaw(demigod, std::as_bytes(std::span(msg)));
    logger->DebugFormatted("[Coordinator] Forwarded client message '{}' to Demigod {}", msg, demigod.ToString());
}

void GameCoordinator::AssignClientToDemigod(const Connection& from)
{
    const auto& allProxies = ProxyRegistry::Get().GetProxies();
    if (allProxies.empty())
    {
        logger->Error("[Coordinator] No proxies registered in ProxyRegistry.");
        return;
    }

    // Simple first-available or round-robin pick
    const auto& selected = allProxies.begin()->second;
    clientToDemigodMap[from.target.ToString()] = selected.identifier;
    logger->DebugFormatted("[Coordinator] Assigned client {} to proxy {} ({})",
                          from.target.ToString(), selected.identifier.ToString(), selected.address.ToString());


    // Optionally notify the Demigod
    std::string connectMsg = "NewClient:" + from.target.ToString();
    Interlink::Get().SendMessageRaw(selected.identifier, std::as_bytes(std::span(connectMsg)));
}

#include "GameCoordinator.hpp"
#include "Docker/DockerIO.hpp"
#include "Database/ProxyRegistry.hpp"
#include "Interlink/InterlinkIdentifier.hpp"

GameCoordinator::GameCoordinator() {}
GameCoordinator::~GameCoordinator() { Shutdown(); }

void GameCoordinator::Init()
{
    // Identify as the GameCoordinator container
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
        AssignClientToDemigod(id);
    }
}

void GameCoordinator::OnMessageReceived(const Connection& from, std::span<const std::byte> data)
{
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[Coordinator] Message from {}: {}", from.target.ToString(), msg);
}

void GameCoordinator::AssignClientToDemigod(const InterLinkIdentifier& clientID)
{
    // Ask ProxyRegistry for the least-loaded proxy
    auto proxyOpt = ProxyRegistry::Get().GetLeastLoadedProxy();
    if (!proxyOpt.has_value())
    {
        logger->Error("[Coordinator] No proxies registered in ProxyRegistry.");
        return;
    }

    const InterLinkIdentifier& proxyID = proxyOpt.value();

    // Record client -> proxy relationship globally and locally
    ProxyRegistry::Get().AssignClientToProxy(clientID.ToString(), proxyID);

    logger->DebugFormatted("[Coordinator] Assigned client {} to proxy {}",
                           clientID.ToString(), proxyID.ToString());

    // Notify the Demigod about the new client
    std::string connectMsg = "NewClient:" + clientID.ToString();
    Interlink::Get().SendMessageRaw(proxyID, std::as_bytes(std::span(connectMsg)));
}
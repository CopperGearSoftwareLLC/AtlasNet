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
                },
                .OnDisconnectedCallback = [this](const InterLinkIdentifier& id)
                {
                    if (id.Type == InterlinkType::eGameClient)
                    {
                      logger->DebugFormatted("[Coordinator] Client {} disconnected.", id.ToString());

                      // Clean up pending handshake, if present
                      pendingClientAssignments.erase(id.ToString());

                      // Clean up ProxyRegistry mappings
                      ProxyRegistry::Get().DecrementClient(id);
                    }
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

    switch (id.Type)
    {
        case InterlinkType::eDemigod:
        {
            //Interlink::Get().CloseConnectionTo(id, 0, "GameCoordinator does not maintain persistent connections to Demigods.");
            break;
        }
        case InterlinkType::eGameClient:
        {
            StartClientProxyHandshake(id);
            break;
        }
    }


}

void GameCoordinator::OnMessageReceived(const Connection& from, std::span<const std::byte> data)
{
    std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
    logger->DebugFormatted("[Coordinator] Message from {}: {}",
                           from.target.ToString(), msg);

    // Only care about GameClient confirmations for now
    if (from.target.Type == InterlinkType::eGameClient)
    {
        const std::string clientKey = from.target.ToString();

        // Simple protocol: client confirms with "ProxyConnected"
        if (msg.rfind("ProxyConnected", 0) == 0)
        {
            auto it = pendingClientAssignments.find(clientKey);
            if (it == pendingClientAssignments.end())
            {
                logger->WarningFormatted(
                    "[Coordinator] Client {} sent ProxyConnected but no pending assignment exists",
                    clientKey);
                return;
            }

            const InterLinkIdentifier& proxyID = it->second;

            // Register the client→proxy mapping in ProxyRegistry
            ProxyRegistry::Get().AssignClientToProxy(clientKey, proxyID);

            logger->DebugFormatted(
                "[Coordinator] Client {} confirmed connection to proxy {}. Registered in ProxyRegistry.",
                clientKey, proxyID.ToString());

            // Notify the Demigod about the new client (same as before)
            std::string connectMsg = "NewClient:" + clientKey;
            Interlink::Get().SendMessageRaw(proxyID, std::as_bytes(std::span(connectMsg)));

            logger->DebugFormatted(
                "[Coordinator] Notified proxy {} about new client {}",
                proxyID.ToString(), clientKey);

            // Remove from pending
            pendingClientAssignments.erase(it);

            // cutting GameCoordinator↔client connection
            Interlink::Get().CloseConnectionTo(proxyID, 0, "Handoff to proxy complete.");

            return;
        }
    }

    // For now, we don't handle other message types specially
    // (log-only behavior above is kept)
}


void GameCoordinator::StartClientProxyHandshake(const InterLinkIdentifier& clientID)
{
    // Ask ProxyRegistry for the least-loaded Demigod
    auto proxyOpt = ProxyRegistry::Get().GetLeastLoadedProxy();
    if (!proxyOpt.has_value())
    {
        logger->Error("[Coordinator] No proxies registered in ProxyRegistry.");
        return;
    }

    const InterLinkIdentifier& proxyID = proxyOpt.value();

    // Try to get the public (host) address of the chosen proxy
    auto publicAddrOpt = ProxyRegistry::Get().GetPublicAddress(proxyID);
    if (!publicAddrOpt.has_value())
    {
        logger->ErrorFormatted("[Coordinator] No public address found for proxy {}",
                               proxyID.ToString());
        return;
    }

    const IPAddress& publicAddr = publicAddrOpt.value();
    const std::string clientKey = clientID.ToString();

    // Remember which proxy we told this client to use
    pendingClientAssignments[clientKey] = proxyID;

    // Send redirect info to client. Client implementation is unknown,
    // so this is a simple text protocol for now:
    //   "ProxyRedirect:<ip:port>"
    std::string redirectMsg = "ProxyRedirect:" + publicAddr.ToString();

    logger->DebugFormatted(
        "[Coordinator] Instructing client {} to connect to proxy {} at {}",
        clientKey, proxyID.ToString(), publicAddr.ToString());

    Interlink::Get().SendMessageRaw(clientID, std::as_bytes(std::span(redirectMsg)));
}

#include "AtlasNetClient.hpp"
#include "../../Database/ServerRegistry.hpp"
#include "../../Docker/DockerIO.hpp"

void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties& props)
{
    CrashHandler::Get().Init(props.ExePath);

    InterLinkIdentifier myID(InterlinkType::eGameClient, DockerIO::Get().GetSelfContainerName());
    logger = std::make_shared<Log>(myID.ToString());
    logger->Debug("AtlasNetClient Initialize");

    Interlink::Check();
    Interlink::Get().Init({
        .ThisID = myID,
        .logger = logger,
        .callbacks = {
            .acceptConnectionCallback = [](const Connection&) { return true; },
            .OnConnectedCallback = [this](const InterLinkIdentifier& id)
            {
                if (id.Type == InterlinkType::eGameServer)
                {
                    connected = true;
                    logger->DebugFormatted("Connected to GameServer {}", id.ToString());
                }
            },
            .OnMessageArrival = [this](const Connection& fromWhom, std::span<const std::byte> data)
            {
                logger->DebugFormatted("[Client] Snapshot {} bytes from {}", data.size(), fromWhom.target.ToString());
            }
        }
    });

    logger->Debug("Waiting for Interlink auto-connect to GameServer...");
}

void AtlasNetClient::Update()
{
    Interlink::Get().Tick();
}

void AtlasNetClient::SendInputIntent(const AtlasEntity& intent)
{
    if (!connected) return;

    const std::byte* ptr = reinterpret_cast<const std::byte*>(&intent);
    std::span<const std::byte> span(ptr, sizeof(AtlasEntity));
    Interlink::Get().SendMessageRaw(serverID, span);
}

void AtlasNetClient::Shutdown()
{
    Interlink::Get().Shutdown();
    connected = false;
    logger.reset();
}

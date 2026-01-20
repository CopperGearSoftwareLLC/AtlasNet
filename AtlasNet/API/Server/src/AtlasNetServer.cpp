#include "AtlasNetServer.hpp"

#include "Crash/CrashHandler.hpp"

#include "DockerIO.hpp"
#include "BuiltInDB.hpp"
// ============================================================================
// Initialize server and setup Interlink callbacks
// ============================================================================
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties &properties)
{

    // --- Core setup ---
    //CrashHandler::Get().Init();
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});

    InterLinkIdentifier myID(InterlinkType::eGameServer,
#ifdef _LOCAL
        std::string("LocalGameServer")
#else
        DockerIO::Get().GetSelfContainerName()
#endif
    );
    logger = std::make_shared<Log>(myID.ToString());
    logger->Debug("AtlasNet Initialize");
    // --- Interlink setup ---
    Interlink::Check();
    Interlink::Get().Init({
        .ThisID = myID,
        .logger = logger,
        .callbacks = {
            .acceptConnectionCallback = [](const Connection &c) { return true; },
            .OnConnectedCallback = [this](const InterLinkIdentifier &id) {
                if (id.Type == InterlinkType::eGameClient)
                {
                    ConnectedClients.insert(id);
                    logger->DebugFormatted("[Server] Client connected: {}", id.ToString());
                }
                else
                {
                    printf("[AtlasNet] Connected to %s\n", id.ToString().c_str());
                }
            }//,
            //.OnMessageArrival = [this](const Connection &fromWhom, std::span<const std::byte> data) {
            //    HandleMessage(fromWhom, data);
            //}
        }
    });

    logger->Debug("Interlink initialized; waiting for auto-connect to Partition...");
}

// ============================================================================
// Update: Called every tick
// Sends entity updates, incoming, and outgoing data to Partition
// ============================================================================
void AtlasNetServer::Update(std::span<AtlasEntity> entities,
                            std::vector<AtlasEntity> &IncomingEntities,
                            std::vector<AtlasEntity::EntityID> &OutgoingEntities)
{
    Interlink::Get().Tick();
    /*
    // ------------------------------------------------------------
    // Step 1: Send regular entity updates (EntityUpdate)
    // ------------------------------------------------------------
    if (!entities.empty())
    {
        std::vector<std::byte> buffer;
        buffer.reserve(1 + entities.size() * sizeof(AtlasEntity));

        buffer.push_back(static_cast<std::byte>(AtlasNetMessageHeader::EntityUpdate));
        for (const auto &entity : entities)
        {
            const std::byte *ptr = reinterpret_cast<const std::byte *>(&entity);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));
            CachedEntities[entity.Client_ID] = entity;
        }

        InterLinkIdentifier partitionID(InterlinkType::eShard, DockerIO::Get().GetSelfContainerName());
        Interlink::Get().SendMessageRaw(partitionID, std::span(buffer), InterlinkMessageSendFlag::eReliableNow);

        logger->DebugFormatted("[Server] Sent EntityUpdate ({} entities) to partition", entities.size());
    }

    // ------------------------------------------------------------
    // Step 2: Send cached incoming entities (EntityIncoming)
    // ------------------------------------------------------------
    if (!IncomingCache.empty())
    {
        IncomingEntities = std::move(IncomingCache);
        logger->DebugFormatted("[Server] Sent EntityIncoming ({} entities) to game", IncomingEntities.size());
        IncomingCache.clear();
    }

    // ------------------------------------------------------------
    // Step 3: Send cached outgoing entities (EntityOutgoing)
    // ------------------------------------------------------------
    if (!OutgoingCache.empty())
    {
        OutgoingEntities = std::move(OutgoingCache);
        logger->DebugFormatted("[Server] Sent EntityOutgoing ({} entities) to game", OutgoingEntities.size());
        OutgoingCache.clear();
    }*/
}

// ============================================================================
// HandleMessage: interpret headers and cache appropriately
// ============================================================================
void AtlasNetServer::HandleMessage(const Connection &fromWhom, std::span<const std::byte> data)
{
    /*
    if (data.size() < 1)
    {
        logger->ErrorFormatted("[Server] Received empty message from {}, ABORT", fromWhom.target.ToString());
        return;
    }

    const AtlasNetMessageHeader header = static_cast<AtlasNetMessageHeader>(data[0]);
    const std::byte *payload = data.data() + 1;
    const size_t payloadSize = data.size() - 1;

    if (payloadSize % sizeof(AtlasEntity) != 0)
    {
        logger->ErrorFormatted("[Server] Invalid payload size {} from {}, ABORT", payloadSize, fromWhom.target.ToString());
        return;
    }

    const size_t entityCount = payloadSize / sizeof(AtlasEntity);
    std::vector<AtlasEntity> entities(entityCount);
    std::memcpy(entities.data(), payload, payloadSize);

    switch (header)
    {
        // --------------------------------------------------------
        // Regular EntityUpdate
        // --------------------------------------------------------
        case AtlasNetMessageHeader::EntityUpdate:
        {
            for (const auto &entity : entities)
                CachedEntities[entity.Client_ID] = entity;

            logger->DebugFormatted("[Server] EntityUpdate: {} entities from {}", entities.size(), fromWhom.target.ToString());
            break;
        }

        // --------------------------------------------------------
        // Cache incoming entities for next update
        // --------------------------------------------------------
        case AtlasNetMessageHeader::EntityIncoming:
        {
            for (const auto &entity : entities)
            {
                CachedEntities[entity.Client_ID] = entity;
                IncomingCache.push_back(entity);
            }
            logger->DebugFormatted("[Server] Cached EntityIncoming: {} entities", entities.size());
            break;
        }

        // --------------------------------------------------------
        // Cache outgoing entities for next update
        // --------------------------------------------------------
        case AtlasNetMessageHeader::EntityOutgoing:
        {
            for (const auto &entity : entities)
            {
                CachedEntities.erase(entity.Client_ID);
                OutgoingCache.push_back(entity.Client_ID);
            }
            logger->DebugFormatted("[Server] Cached EntityOutgoing: {} entities", entities.size());
            break;
        }

        default:
            logger->ErrorFormatted("[Server] Unknown AtlasNetMessageHeader {} from {}",
                                   static_cast<int>(header), fromWhom.target.ToString());
            return;
    }

    return;
    // --------------------------------------------------------
    // Rebroadcast to clients as before
    // --------------------------------------------------------
    if (fromWhom.target.Type == InterlinkType::eGameClient)
    {
        for (const auto &id : ConnectedClients)
        {
            if (id != fromWhom.target)
                Interlink::Get().SendMessageRaw(id, data);
        }
    }
    else if (fromWhom.target.Type == InterlinkType::eShard)
    {
        for (const auto &id : ConnectedClients)
            Interlink::Get().SendMessageRaw(id, data);
    }*/
}
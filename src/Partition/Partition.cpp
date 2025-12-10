#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ShapeManifest.hpp"
#include "Database/GridCellManifest.hpp"
#include "Database/EntityManifest.hpp"
#include "Database/PartitionEntityManifest.hpp"
#include "Utils/GeometryUtils.hpp"
#include "Heuristic/Shape.hpp"

Partition::Partition()
{
    database = std::make_unique<RedisCacheDatabase>();
    if (!database->Connect())
        logger->Error("Failed to connect to database in Partition constructor");

    lastOutlierNotification = std::chrono::steady_clock::now();
    lastEntitiesSnapshotPush = std::chrono::steady_clock::now();
}

Partition::~Partition() {}

InterLinkIdentifier Partition::getCurrentPartitionId() const
{
    return InterLinkIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
}

void Partition::Init()
{
    InterLinkIdentifier partitionIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
    logger = std::make_shared<Log>(partitionIdentifier.ToString());

    Interlink::Get().Init(
        InterlinkProperties{
            .ThisID = partitionIdentifier,
            .logger = logger,
            .callbacks = {
                .acceptConnectionCallback = [](const Connection &c) { return true; },
                .OnConnectedCallback = [this](const InterLinkIdentifier &Connection) {
                    if (Connection.Type == InterlinkType::eGameServer)
                        ConnectedGameServer = std::make_unique<InterLinkIdentifier>(Connection);
                    else if (Connection.Type == InterlinkType::eDemigod)
                        ConnectedProxies.insert(Connection);
                },
                .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {
                    Partition::Get().MessageArrived(fromWhom, data);
                },
                .OnDisconnectedCallback = [this](const InterLinkIdentifier &Connection) {
                    if (Connection.Type == InterlinkType::eGameServer)
                        ConnectedGameServer = nullptr;
                    else if (Connection.Type == InterlinkType::eDemigod)
                        ConnectedProxies.erase(Connection);
                }
            }
        });

    // Reset state on boot
    PartitionEntityManifest::ClearPartition(database.get(), partitionIdentifier.ToString());

    // Hello God
    std::string testMessage = "Hello from " + partitionIdentifier.ToString();
    Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(testMessage)), InterlinkMessageSendFlag::eReliableNow);

    logger->DebugFormatted("[Partition] Initialized as {}", partitionIdentifier.ToString());

    while (!ShouldShutdown)
    {
        checkForOutliersAndNotifyGod();
        notifyGodAboutOutliers();
        pushManagedEntitiesSnapshot();
        Interlink::Get().Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(32)); 
    }

    Interlink::Get().Shutdown();
}

// ============================================================================
// Core Logic: Message Handling
// ============================================================================
void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{
    if (data.size() < 1) return;

    AtlasNetMessageHeader header = static_cast<AtlasNetMessageHeader>(data[0]);
    std::vector<AtlasEntity> incomingEntities;
    bool isEntityPacket = ParseEntityPacket(data, header, incomingEntities);

    // --- 1. Binary Packet Handling ---
    if (isEntityPacket)
    {
        switch (header)
        {
        case AtlasNetMessageHeader::EntityUpdate:
            // Just update local state, no DB/Network changes needed for high-freq updates
            for (const auto &entity : incomingEntities)
            {
                auto it = managedEntities.find(entity.ID);
                if (it != managedEntities.end())
                {
                    it->second = entity;   // Update existing entity safely
                }
            }
            return;
            break;

        case AtlasNetMessageHeader::EntityIncoming:
            for (const auto &entity : incomingEntities)
            {
                if (fromWhom.target.Type == InterlinkType::eDemigod)
                {
                    // Proxy Request: Check bounds, Claim, Notify Sender
                    if (IsEntityInside(entity))
                    {
                        RegisterEntity(entity);
                        NotifyProxiesIncoming(entity, &fromWhom.target); 
                        logger->DebugFormatted("[Partition] Claimed entity {} from Proxy", entity.ID);
                    }
                }
                else if (fromWhom.target.Type == InterlinkType::eGod)
                {
                    // God Command: Force accept
                    RegisterEntity(entity);
                    logger->DebugFormatted("[Partition] Forced accept entity {} from God", entity.ID);
                }
            }
            break;

        case AtlasNetMessageHeader::EntityOutgoing:
            for (const auto &entity : incomingEntities)
            {
                UnregisterEntity(entity.ID);
                logger->DebugFormatted("[Partition] Removed entity {} (Binary instruction)", entity.ID);
            }
            break;
        default: break;
        }

        // Broadcast visual updates
        for (const auto &proxy : ConnectedProxies)
            Interlink::Get().SendMessageRaw(proxy, data, InterlinkMessageSendFlag::eUnreliableNow);
        
        if (ConnectedGameServer)
            Interlink::Get().SendMessageRaw(*ConnectedGameServer, data, InterlinkMessageSendFlag::eUnreliableNow);
    }

    // --- 2. String/Control Message Handling ---
    if (!isEntityPacket && data.size() > 1)
    {
        const std::byte *payload = data.data() + 1;
        std::string msg(reinterpret_cast<const char *>(payload), data.size() - 1);

        switch (header)
        {
        case AtlasNetMessageHeader::EntityIncoming: // "sourcePartition:entityId"
        {
            size_t colon = msg.find(':');
            if (colon == std::string::npos || !database) return;

            std::string srcPart = msg.substr(0, colon);
            AtlasEntityID eid = std::stoul(msg.substr(colon + 1));

            // Fetch from DB (Source of Truth for transfers)
            auto outliers = EntityManifest::FetchOutliers(database.get(), srcPart);
            auto it = std::find_if(outliers.begin(), outliers.end(), [eid](auto &e){ return e.ID == eid; });

            if (it != outliers.end())
            {
                RegisterEntity(*it);
                NotifyProxiesIncoming(*it); // Broadcast to all proxies
                logger->DebugFormatted("FETCHED: Claimed {} from {}", eid, srcPart);
            }
            break;
        }
        case AtlasNetMessageHeader::EntityOutgoing: // "entityId"
        {
            AtlasEntityID eid = std::stoul(msg);
            if (managedEntities.contains(eid))
            {
                // Unregister first
                AtlasEntity e = managedEntities[eid];
                UnregisterEntity(eid);

                // Then Ack to God
                std::string info = "EntityRemoved:" + getCurrentPartitionId().ToString() + ":" +
                                   std::to_string(e.ID) + ":" + std::to_string(e.Position.x) + ":" +
                                   std::to_string(e.Position.y) + ":" + std::to_string(e.Position.z);
                Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(info)), InterlinkMessageSendFlag::eReliableNow);
            }
            break;
        }
        case AtlasNetMessageHeader::FetchGridShape:
        {
            if (!database) return;
            auto data = GridCellManifest::Fetch(database.get(), getCurrentPartitionId().ToString());
            if (data) {
                try {
                    partitionGridShape = GridCellManifest::ParseGridShape(*data);
                    reportedOutliers.clear();
                    logger->DebugFormatted("Loaded grid shape: {} cells", partitionGridShape.cells.size());
                } catch(...) {}
            }
            break;
        }
        default: break;
        }
    }
}

// ============================================================================
// Logic Helpers (Consolidated)
// ============================================================================

void Partition::RegisterEntity(const AtlasEntity& entity)
{
    // 1. Update Memory
    managedEntities[entity.ID] = entity;
    
    // 2. Update Database Manifest
    if (database)
        PartitionEntityManifest::AddEntity(database.get(), getCurrentPartitionId().ToString(), entity);
    
    // 3. Clear Outlier Status (We own it now)
    reportedOutliers.erase(entity.ID);
    logger->DebugFormatted("[Partition] registered entity {} to cache + database", entity.ID);
}

void Partition::UnregisterEntity(AtlasEntityID id)
{
    // 1. Update Memory
    managedEntities.erase(id);
    
    // 2. Update Database Manifest
    if (database)
        PartitionEntityManifest::RemoveEntity(database.get(), getCurrentPartitionId().ToString(), id);
    
    // 3. Clean up tracking
    reportedOutliers.erase(id);
    logger->DebugFormatted("[Partition] deregistered entity {} to cache + database", id);
}

void Partition::checkForOutliersAndNotifyGod()
{
    if ((partitionShape.triangles.empty() && partitionGridShape.cells.empty()) || managedEntities.empty()) return;

    std::vector<AtlasEntity> newOutliers;

    for (const auto& [id, entity] : managedEntities)
    {
        bool isOut = !IsEntityInside(entity);
        if (isOut)
        {
            if (reportedOutliers.find(id) == reportedOutliers.end())
            {
                newOutliers.push_back(entity);
                reportedOutliers.insert(id); // Mark as reported
                logger->DebugFormatted("[Partition] Entity {} moved OUTSIDE.", id);
            }
        }
        else
        {
            if (reportedOutliers.erase(id)) // Was outside, now inside
                logger->DebugFormatted("[Partition] Entity {} returned INSIDE.", id);
        }
    }

    if (!newOutliers.empty())
    {
        // 1. Store in DB (So neighbor can fetch it)
        if (database)
            EntityManifest::StoreOutliers(database.get(), getCurrentPartitionId().ToString(), newOutliers);

        // 2. Notify God (Persistent logic)
        NotifyGodOfOutliers(newOutliers);

        // 3. Notify Proxies (Visuals/Routing)
        // Tell proxies we are "Done" with this entity so they stop routing client updates to us.
        NotifyProxiesOutgoing(newOutliers);
        
        lastOutlierNotification = std::chrono::steady_clock::now();
    }
}

void Partition::NotifyProxiesIncoming(const AtlasEntity& entity, const InterLinkIdentifier* specificTarget)
{
    // Construct Packet
    std::vector<std::byte> buffer;
    buffer.reserve(1 + sizeof(AtlasEntity));
    buffer.push_back(static_cast<std::byte>(AtlasNetMessageHeader::EntityIncoming));
    const std::byte* ptr = reinterpret_cast<const std::byte*>(&entity);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));

    auto span = std::span(buffer);

    if (specificTarget)
    {
        Interlink::Get().SendMessageRaw(*specificTarget, span, InterlinkMessageSendFlag::eReliableNow);
    }
    else
    {
        for (const auto &proxy : ConnectedProxies)
            Interlink::Get().SendMessageRaw(proxy, span, InterlinkMessageSendFlag::eReliableNow);
    }
}

void Partition::NotifyProxiesOutgoing(const std::vector<AtlasEntity>& entities)
{
    if (entities.empty()) return;

    std::vector<std::byte> buffer;
    buffer.reserve(1 + entities.size() * sizeof(AtlasEntity));
    buffer.push_back(static_cast<std::byte>(AtlasNetMessageHeader::EntityOutgoing));
    
    for (const auto& entity : entities)
    {
        const std::byte* ptr = reinterpret_cast<const std::byte*>(&entity);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));
    }

    for (const auto &proxy : ConnectedProxies)
        Interlink::Get().SendMessageRaw(proxy, std::span(buffer), InterlinkMessageSendFlag::eReliableNow);
}

void Partition::NotifyGodOfOutliers(const std::vector<AtlasEntity>& outliers)
{
    std::string notifyMsg = "OutliersDetected:" + getCurrentPartitionId().ToString() + ":" + std::to_string(outliers.size());
    Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(), std::as_bytes(std::span(notifyMsg)), InterlinkMessageSendFlag::eReliableNow);
}

// ... (Existing Helpers: notifyGodAboutOutliers, pushManagedEntitiesSnapshot, IsEntityInside, ParseEntityPacket - Keep as is) ...

void Partition::notifyGodAboutOutliers()
{
    if (reportedOutliers.empty()) return;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastOutlierNotification).count() < 30) return;

    std::vector<AtlasEntity> dummyList(reportedOutliers.size()); // Just for count
    NotifyGodOfOutliers(dummyList); 
    lastOutlierNotification = now;
}

void Partition::pushManagedEntitiesSnapshot()
{
    std::vector<AtlasEntity> snapshot;
    snapshot.reserve(managedEntities.size());
    for(const auto& [id, entity] : managedEntities) snapshot.push_back(entity);

    EntityManifest::PushManagedEntitiesSnapshot(
        database.get(), getCurrentPartitionId().ToString(), snapshot, lastEntitiesSnapshotPush, logger
    );
}

bool Partition::IsEntityInside(const AtlasEntity& entity)
{
    vec2 pos{entity.Position.x, entity.Position.z};
    if (!partitionGridShape.cells.empty()) return partitionGridShape.contains(pos);
    for (const auto &tri : partitionShape.triangles)
        if (GeometryUtils::PointInTriangle(pos, tri[0], tri[1], tri[2])) return true;
    return false;
}

bool Partition::ParseEntityPacket(std::span<const std::byte> data, AtlasNetMessageHeader &outHeader, std::vector<AtlasEntity> &outEntities)
{
    if (data.size() < 1) return false;
    outHeader = static_cast<AtlasNetMessageHeader>(data[0]);
    if (outHeader != AtlasNetMessageHeader::EntityUpdate && 
        outHeader != AtlasNetMessageHeader::EntityIncoming && 
        outHeader != AtlasNetMessageHeader::EntityOutgoing) return false;

    size_t payloadSize = data.size() - 1;
    if (payloadSize == 0 || payloadSize % sizeof(AtlasEntity) != 0) return false;

    outEntities.resize(payloadSize / sizeof(AtlasEntity));
    std::memcpy(outEntities.data(), data.data() + 1, payloadSize);
    return true;
}
#include "AtlasNetServer.hpp"

#include "Debug/Crash/CrashHandler.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties &properties)
{
    // --- Core setup ---
    CrashHandler::Get().Init(properties.ExePath);
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = properties.OnShutdownRequest});

    InterLinkIdentifier myID(InterlinkType::eGameServer, DockerIO::Get().GetSelfContainerName());
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
                    printf("[AtlasNet] Connected to %s\n", id.ToString().c_str());
            },
            .OnMessageArrival = [this](const Connection &fromWhom, std::span<const std::byte> data) {
                //std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
                //printf("[AtlasNet] Received: %s\n", msg.c_str());
                HandleMessage(fromWhom, data);
            }
        }
    });

    // --- Connection logging only ---
    logger->Debug("Interlink initialized; waiting for auto-connect to Partition...");
}


void AtlasNetServer::Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
							std::vector<AtlasEntityID> &OutgoingEntities)
{
  Interlink::Get().Tick();
  
  // If no entities, skip sending
  if (entities.empty()) return;
  
  // Serialize snapshot to bytes
  std::vector<std::byte> buffer;
  buffer.reserve(entities.size() * sizeof(AtlasEntity));
  
  for (const auto& entity : entities)
  {
    const std::byte* ptr = reinterpret_cast<const std::byte*>(&entity);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(AtlasEntity));

    CachedEntities[entity.ID] = entity;
  }

    if (CachedEntities.empty())
        return;

    // forward to partition
    std::vector<AtlasEntity> snapshot;
    snapshot.reserve(CachedEntities.size());
    for (auto &[id, e] : CachedEntities)
        snapshot.push_back(e);

    InterLinkIdentifier partitionID(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
    Interlink::Get().SendMessageRaw(partitionID, std::as_bytes(std::span(snapshot)));

    //logger->DebugFormatted("[Server] Sent snapshot of {} entities to partition", snapshot.size());
}

void AtlasNetServer::HandleMessage(const Connection &fromWhom, std::span<const std::byte> data)
{

    AtlasEntity entity{};
    if (data.size() == sizeof(AtlasEntity))
    {
        std::memcpy(&entity, data.data(), sizeof(AtlasEntity));

        CachedEntities[entity.ID] = entity;

        logger->DebugFormatted("[Server] Received transform from client {} (Entity {}) -> Pos({:.2f}, {:.2f}, {:.2f})",
            fromWhom.target.ToString(), entity.ID, entity.Position.x, entity.Position.y, entity.Position.z);
    }
    else
    {
      logger->ErrorFormatted("[Server] Received non-AtlasEntity message size {} from {}, aborting", data.size(), fromWhom.target.ToString());
      return;
    }

    switch (fromWhom.target.Type)
    {
      case InterlinkType::eGameClient:
        // rebroadcast to other clients
        for (const auto& id : ConnectedClients)
        {
            if (id != fromWhom.target)
            {
                Interlink::Get().SendMessageRaw(id, std::as_bytes(std::span(&entity, 1)));
            }
        }
        break;
        case InterlinkType::ePartition:
        
        break;
      default:
        logger->ErrorFormatted("[Server] Received message from unknown InterlinkType {} from {}", static_cast<int>(fromWhom.target.Type), fromWhom.target.ToString());
    };
}
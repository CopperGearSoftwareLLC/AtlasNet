#include "AtlasNetServer.hpp"

#include "Interlink/Interlink.hpp"
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
            .OnConnectedCallback = [](const InterLinkIdentifier &id) {
                printf("[AtlasNet] Connected to %s\n", id.ToString().c_str());
            },
            .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {
                std::string msg(reinterpret_cast<const char*>(data.data()), data.size());
                printf("[AtlasNet] Received: %s\n", msg.c_str());
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
  }
  
  
  // Send snapshot to Partition
  InterLinkIdentifier partitionID(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
  //logger->DebugFormatted("try send snapshot with entities {} to {}", entities.size(), partitionID.ToString());
  Interlink::Get().SendMessageRaw(partitionID, std::as_bytes(std::span(buffer)));

  //logger->DebugFormatted("Sent snapshot with {} entities to {}", entities.size(), partitionID.ToString());
}

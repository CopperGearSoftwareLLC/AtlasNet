#include "God.hpp"
#include "Docker/DockerIO.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include <ctime>
#include "Database/ServerRegistry.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "Interlink/Connection.hpp"

#include "AtlasNet/AtlasNetBootstrap.hpp"
God::God()
{
}

God::~God()
{
}

void God::Init()
{
  logger->Debug("Init");

  InterlinkProperties InterLinkProps;
  InterLinkProps.callbacks = InterlinkCallbacks{
      .acceptConnectionCallback = [](const Connection &c)
      { return true; },
      .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
      .OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data) {},
  };
  InterLinkProps.logger = logger;
  InterLinkProps.ThisID = InterLinkIdentifier::MakeIDGod();
  Interlink::Get().Init(InterLinkProps);

  SetPartitionCount(10);
  // std::this_thread::sleep_for(std::chrono::seconds(4));
  // god.removePartition(4);
  using clock = std::chrono::steady_clock;
  auto startTime = clock::now();
  auto lastCall = startTime;
  bool firstCalled = false;
  while (!ShouldShutdown)
  {
    auto now = clock::now();
    Interlink::Get().Tick();
    if (!firstCalled && now - startTime >= std::chrono::seconds(10))
    {
      firstCalled = true;
      HeuristicUpdate();
    }
    if (false && firstCalled && now - lastCall >= std::chrono::seconds(1))
    {
      HeuristicUpdate();
      lastCall = now;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  logger->Debug("Shutting down");
  Cleanup();
  Interlink::Get().Shutdown();
}

void God::HeuristicUpdate()
{
  if (computeAndStorePartitions())
  {
    notifyPartitionsToFetchShapes();
  }
}

std::vector<std::string> God::GetPartitionIDs()
{
  std::string serviceResp = DockerIO::Get().request("GET", "/services/"+AtlasNetBootstrap::Get().PartitionServiceName);
  Json serviceJson = Json::parse(serviceResp);
  std::string serviceID = serviceJson["ID"];

  std::string tasksResp = DockerIO::Get().request("GET", "/tasks?filters={\"service\":{\"" + serviceID + "\":true}}");
  Json tasksJson = Json::parse(tasksResp);
  std::vector<std::string> instanceNames;
  for (auto &task : tasksJson)
  {
    if (task.contains("Status") && task["Status"].contains("ContainerStatus"))
    {
      auto &cstatus = task["Status"]["ContainerStatus"];
      if (cstatus.contains("ContainerID"))
      {
        std::string containerID = cstatus["ContainerID"];
        // Optional: inspect the container to get its name
        std::string contResp = DockerIO::Get().request("GET", "/containers/" + containerID + "/json");
        Json contJson = Json::parse(contResp);
        if (contJson.contains("Name"))
          instanceNames.push_back(contJson["Name"].get<std::string>());
      }
    }
  }
  for (auto &name : instanceNames)
    std::cout << "Instance: " << name << std::endl;
  return instanceNames;
}

void God::notifyPartitionsToFetchShapes()
{
  for (const auto &container : ServerRegistry::Get().GetServers())
  {
    if (container.first.Type != InterlinkType::ePartition)
      continue;
    InterLinkIdentifier id = container.first;
    std::string Fetch = "Fetch Shape";
    logger->DebugFormatted("Sending shape notify to {}", id.ToString());
    Interlink::Get().SendMessageRaw(id, std::as_bytes(std::span(Fetch)));
  }
}

void God::Cleanup()
{
}

void God::SetPartitionCount(uint32 NewCount)
{
  std::string inspectResp = DockerIO::Get().request("GET", "/services/"+AtlasNetBootstrap::Get().PartitionServiceName);
  auto inspectJson = Json::parse(inspectResp);

  try
  {
    int version = inspectJson["Version"]["Index"];
    auto spec = inspectJson["Spec"];
    spec["Mode"]["Replicated"]["Replicas"] = NewCount;
    // 2. Send the update request with the new replica count
    std::string updatePath = "/services/"+AtlasNetBootstrap::Get().PartitionServiceName+"/update?version=" + std::to_string(version);
    std::string updateResp = DockerIO::Get().request("POST", updatePath, &spec);
    if (!updateResp.empty())
    {
      logger->DebugFormatted("Service update responded with \n{}",Json::parse(updateResp).dump(4));
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    logger->ErrorFormatted("Response from service inspect \n{}", inspectJson.dump(4));
    throw "SHIT";
  }

  logger->DebugFormatted("Scaled {} to {} replicas",AtlasNetBootstrap::Get().PartitionServiceName, NewCount);
  PartitionCount = NewCount;
}

bool God::computeAndStorePartitions()
{
  try
  {
    // Compute partition shapes using heuristic algorithms
    std::vector<Shape> partitionShapes = heuristic.computePartition();

    logger->DebugFormatted("Computed {} partition shapes", partitionShapes.size());

    // Initialize cache database if not already done
    if (!cache)
    {
      cache = std::make_unique<RedisCacheDatabase>();
      if (!cache->Connect())
      {
        logger->Error("Failed to connect to cache database");
        return false;
      }
    }

    // Get all partition servers from registry
    const auto &servers = ServerRegistry::Get().GetServers();
    std::vector<InterLinkIdentifier> partitionIds;

    // Filter to get only partition IDs
    for (const auto &server : servers)
    {
      if (server.first.Type == InterlinkType::ePartition)
      {
        partitionIds.push_back(server.first);
      }
    }

    // Sort partition IDs to ensure consistent assignment
    std::sort(partitionIds.begin(), partitionIds.end());

    size_t numShapesToAssign = std::min(partitionShapes.size(), partitionIds.size());
    logger->DebugFormatted("Assigning {} shapes to {} partitions", numShapesToAssign, partitionIds.size());

    // Serialize and store shapes, mapping them to partition IDs
    for (size_t i = 0; i < numShapesToAssign; ++i)
    {
      std::string shapeData;
      shapeData += "shape_id:" + std::to_string(i) + ";";
      shapeData += "partition_id:" + partitionIds[i].ToString() + ";";
      shapeData += "triangles:";

      // Serialize triangles as simple string format
      for (const auto &triangle : partitionShapes[i].triangles)
      {
        shapeData += "triangle:";
        for (const auto &vertex : triangle)
        {
          shapeData += "v(" + std::to_string(vertex.x) + "," + std::to_string(vertex.y) + ");";
        }
      }

      if (!cache->HashSet(m_PartitionShapeManifest, partitionIds[i].ToString(), shapeData))
      {
        logger->ErrorFormatted("Failed to store shape for partition {} in database", partitionIds[i].ToString());
        return false;
      }

      logger->DebugFormatted("Stored shape for partition {} with {} triangles", partitionIds[i].ToString(), partitionShapes[i].triangles.size());
    }

    // If there's only one shape, log that other partitions are ignored
    if (partitionShapes.size() == 1 && partitionIds.size() > 1)
    {
      logger->Debug("Only one shape available - assigned to first partition, remaining partitions ignored");
    }

    // Store metadata about the partition computation
    std::string metadata = "total_shapes:" + std::to_string(numShapesToAssign) +
                           ";total_partitions:" + std::to_string(partitionIds.size()) +
                           ";timestamp:" + std::to_string(std::time(nullptr));

    if (!cache->Set("shape_manifest_metadata", metadata))
    {
      logger->Error("Failed to store partition metadata");
      return false;
    }

    logger->Debug("Successfully computed and stored all partition shapes");
    return true;
  }
  catch (const std::exception &e)
  {
    logger->ErrorFormatted("Error in computeAndStorePartitions: {}", e.what());
    return false;
  }
}

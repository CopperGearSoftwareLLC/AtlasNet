#include "God.hpp"
#include "Docker/DockerIO.hpp"
God::God()
{
}

God::~God()
{
}

void God::Init()
{
  logger->Print("Init");
  InterlinkProperties InterLinkProps;
  InterLinkProps.acceptConnectionFunc = [](const Connection &)
  { return true; };
  InterLinkProps.ListenSocketPort = 1234;
  InterLinkProps.logger = logger;
  InterLinkProps.ThisID = InterLinkIdentifier::MakeIDGod();
  InterLinkProps.bOpenListenSocket = true;
  Interlink::Get().Init(InterLinkProps);
  for (int32 i = 1; i <= 12; i++)
  {

    spawnPartition();
  }

  // std::this_thread::sleep_for(std::chrono::seconds(4));
  // god.removePartition(4);

  while (!ShouldShutdown)
  {
  }

  logger->Print("Shutting down");
  cleanupContainers();
}

const decltype(God::ActiveContainers) &God::GetContainers()
{
  return ActiveContainers;
}

const God::ActiveContainer &God::GetContainer(const DockerContainerID &id)
{

  const ActiveContainer &v = *ActiveContainers.get<IndexByID>().find(id);
  return v;
}

std::optional<God::ActiveContainer> God::spawnPartition()
{
  Json createRequestBody =
      {
          {"Image", "partition"},
          //{"Cmd", {"--testABCD"}},
          {"ExposedPorts", {}},
          {"HostConfig", {{"PublishAllPorts", true}}}};

  std::string createResp = DockerIO::Get().request("POST", "/containers/create", &createRequestBody);

  auto createRespJ = nlohmann::json::parse(createResp);

  ActiveContainer newPartition;
  // logger->Print(createRespJ.dump(4));
  newPartition.ID = createRespJ["Id"].get<std::string>();
  newPartition.LatestInformJson = nlohmann::json::parse(DockerIO::Get().InspectContainer(newPartition.ID));
  std::string StartResponse = DockerIO::Get().request("POST", std::string("/containers/").append(newPartition.ID).append("/start"));
  logger->PrintFormatted("Created container with ID {}", newPartition.ID); //, newPartition.LatestInformJson.dump(4));
  ActiveContainers.insert(newPartition);

  return newPartition;
}

bool God::removePartition(const DockerContainerID &id, uint32 TimeOutSeconds)
{
  logger->PrintFormatted("Deleting {}", id);
  std::string RemoveResponse = DockerIO::Get().request("POST", "/containers/" + std::string(id) + "/stop?t=" + std::to_string(TimeOutSeconds));
  std::string DeleteResponse = DockerIO::Get().request("DELETE", "/containers/" + id);

  return true;
}

bool God::cleanupContainers()
{

  for (const auto &Partition : ActiveContainers)
  {
    logger->PrintFormatted("Deleting {}", Partition.ID);
    removePartition(Partition.ID);
  }
  ActiveContainers.clear();
  return true;
}

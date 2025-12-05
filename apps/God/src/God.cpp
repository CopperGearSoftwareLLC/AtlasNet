#include "God.hpp"

#include <ctime>

#include "Database/EntityManifest.hpp"
#include "Database/GridCellManifest.hpp"
#include "Database/PartitionEntityManifest.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ServerRegistry.hpp"
#include "Database/ShapeManifest.hpp"
#include "Docker/DockerIO.hpp"
#include "Interlink/Connection.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "misc/GeometryUtils.hpp"
God::God() {}

God::~God() {}

void God::ClearAllDatabaseState()
{
	// Ensure cache is initialized
	if (!cache)
	{
		cache = std::make_unique<RedisCacheDatabase>();
		cache->Connect();
	}
	logger->Debug("Clearing all entity and shape data from database and memory cache");
	ShapeManifest::Clear(cache.get());
	GridCellManifest::Clear(cache.get());  // Clear grid cell data too
	EntityManifest::ClearAllOutliers(cache.get());

	// Clear ALL partition entity manifests to prevent stale data from previous runs
	PartitionEntityManifest::ClearAllPartitionManifests(cache.get());

	// Clear memory cache
	shapeCache.shapes.clear();
	shapeCache.partitionToShapeIndex.clear();
	shapeCache.isValid = false;
}

void God::Init()
{
	logger->Debug("Init");
	ClearAllDatabaseState();
	InterlinkProperties InterLinkProps;
	InterLinkProps.callbacks = InterlinkCallbacks{
		.acceptConnectionCallback = [](const Connection &c) { return true; },
		.OnConnectedCallback = [](const InterLinkIdentifier &Connection) {},
		.OnMessageArrival =
			[](const Connection &fromWhom, std::span<const std::byte> data)
		{
			std::string msg(reinterpret_cast<const char *>(std::data(data)), std::size(data));
			God::Get().handleOutlierMessage(msg);
		},
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
		// First call after 10 seconds to compute initial partition shapes
		if (!firstCalled && now - startTime >= std::chrono::seconds(10))
		{
			firstCalled = true;
			HeuristicUpdate();
			// Process any existing outliers in the database after shapes are computed
			processExistingOutliers();
		}
		// wait for partion messages for redistribution
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	logger->Debug("Shutting down");
	Cleanup();
	Interlink::Get().Shutdown();
}

void God::HeuristicUpdate()
{
	computeAndStorePartitions();
	notifyPartitionsToFetchShapes();
}

std::vector<std::string> God::GetPartitionIDs()
{
	const std::string PartitionAccessName = std::string(_ATLASNET_STACK_NAME) +"_"+std::string(_PARTITION_SERVICE_NAME);
	std::string serviceResp = DockerIO::Get().request(
		"GET", "/services/" + PartitionAccessName);
	Json serviceJson = Json::parse(serviceResp);
	std::string serviceID = serviceJson["ID"];

	std::string tasksResp =
		DockerIO::Get().request("GET", R"(/tasks?filters={"service":{")" + serviceID + "\":true}}");
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
				std::string contResp =
					DockerIO::Get().request("GET", "/containers/" + containerID + "/json");
				Json contJson = Json::parse(contResp);
				if (contJson.contains("Name"))
					instanceNames.push_back(contJson["Name"].get<std::string>());
			}
		}
	}
	for (auto &name : instanceNames) std::cout << "Instance: " << name << std::endl;
	return instanceNames;
}

void God::Cleanup() {}

void God::SetPartitionCount(uint32 NewCount)
{
	const std::string PartitionAccessName = std::string(_ATLASNET_STACK_NAME) +"_"+std::string(_PARTITION_SERVICE_NAME);

	std::string inspectResp = DockerIO::Get().request(
		"GET", "/services/" + PartitionAccessName);
	auto inspectJson = Json::parse(inspectResp);

	try
	{
		int version = inspectJson["Version"]["Index"];
		auto spec = inspectJson["Spec"];
		spec["Mode"]["Replicated"]["Replicas"] = NewCount;
		// 2. Send the update request with the new replica count
		std::string updatePath = "/services/" + PartitionAccessName +
								 "/update?version=" + std::to_string(version);
		std::string updateResp = DockerIO::Get().request("POST", updatePath, &spec);
		if (!updateResp.empty())
		{
			logger->DebugFormatted("Service update responded with \n{}",
								   Json::parse(updateResp).dump(4));
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << '\n';
		logger->ErrorFormatted("Response from service inspect \n{}", inspectJson.dump(4));
		throw "SHIT";
	}

	logger->DebugFormatted("Scaled {} to {} replicas",
						  _PARTITION_SERVICE_NAME, NewCount);
	PartitionCount = NewCount;
}
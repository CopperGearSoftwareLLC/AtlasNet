#include "WatchDog.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <stop_token>
#include <thread>

#include "Connection.hpp"
#include "Database/HealthManifest.hpp"
#include "Database/HeuristicManifest.hpp"
#include "DockerIO.hpp"
#include "Entity.hpp"
#include "EntityList.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Interlink.hpp"
#include "InterlinkEnums.hpp"
#include "InterlinkIdentifier.hpp"
#include "Packet/CommandPacket.hpp"
#include "Packet/RelayPacket.hpp"
#include "Transform.hpp"
WatchDog::WatchDog() {}

WatchDog::~WatchDog() {}

void WatchDog::ClearAllDatabaseState()
{
	// Ensure cache is initialized

	logger->Debug("Clearing all entity and shape data from database and memory cache");
}
void WatchDog::Run()
{
	Init();
	while (!ShouldShutdown)
	{
		Interlink::Get().Tick();
	}
	logger->Debug("Shutting down");
	Cleanup();
}
void WatchDog::Init()
{
	logger->Debug("Init");
	HealthManifest::Get().ScheduleHealthPings(ID);
	HealthManifest::Get().ScheduleHealthChecks(
		[&](const InterLinkIdentifier &ID_fail, const std::string &key)
		{
			logger->ErrorFormatted("HEALTH CHECK FAIL : {}. Removing from Health Manifest",
								   ID_fail.ToString());
			HealthManifest::Get().RemovePing(key);
		});
	InterlinkProperties InterLinkProps;
	InterLinkProps.callbacks = InterlinkCallbacks{
		.acceptConnectionCallback = [](const Connection &c) { return true; },
		.OnConnectedCallback = [](const InterLinkIdentifier &Connection) {}	 //,
		//.OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data)
		//{ std::string msg(reinterpret_cast<const char *>(std::data(data)), std::size(data)); },
	};
	InterLinkProps.logger = logger;
	InterLinkProps.ThisID = ID;
	Interlink::Get().Init(InterLinkProps);

	SwitchHeuristic(IHeuristic::Type::eGridCell);
	HeuristicThread = std::jthread([this](std::stop_token st) { this->HeuristicThreadEntry(st); });

	while (!ShouldShutdown)
	{
		Interlink::Get().Tick();
	}
	logger->Debug("Shutting down");
	Cleanup();
	Interlink::Get().Shutdown();


}

void WatchDog::Cleanup()
{
	Interlink::Get().Shutdown();
}

void WatchDog::SetShardCount(uint32 NewCount)
{
	const std::string PartitionAccessName =
		std::string(_ATLASNET_STACK_NAME) + "_" + std::string(_SHARD_SERVICE_NAME);

	std::string inspectResp = DockerIO::Get().request("GET", "/services/" + PartitionAccessName);
	auto inspectJson = Json::parse(inspectResp);

	try
	{
		int version = inspectJson["Version"]["Index"];
		auto spec = inspectJson["Spec"];
		spec["Mode"]["Replicated"]["Replicas"] = NewCount;
		// 2. Send the update request with the new replica count
		std::string updatePath =
			"/services/" + PartitionAccessName + "/update?version=" + std::to_string(version);
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

	logger->DebugFormatted("Scaled {} to {} replicas", _SHARD_SERVICE_NAME, NewCount);
	ShardCount = NewCount;
}
void WatchDog::ComputeHeuristic()
{
	AtlasEntityList<AtlasEntityMinimal> entities;
	for (int i = 0; i < 4; i++)
	{
		AtlasEntityMinimal e;
		e.transform.position = {10 * ((i / 2) ? 1 : -1), 10 * ((i % 2) ? 1 : -1), 0};
		entities.push_back(e);
	}
	logger->DebugFormatted("Computing Heuristic: {}", IHeuristic::TypeToString(ActiveHeuristic));

	HeuristicManifest::Get().SetActiveHeuristicType(ActiveHeuristic);
	Heuristic->Compute(entities.span());
	std::unordered_map<IBounds::BoundsID, ByteWriter> bws;
	Heuristic->SerializeBounds(bws);
	HeuristicManifest::Get().StorePendingBoundsFromByteWriters(bws);

	std::vector<std::string> data;
	std::unordered_map<IBounds::BoundsID, ByteReader> brs;
	HeuristicManifest::Get().GetPendingBoundsAsByteReaders(data, brs);

	for (auto &[boundID, bound_reader] : brs)
	{
		GridShape s;
		s.Deserialize(bound_reader);
		logger->DebugFormatted("Retreived ID {}, min:{}, max:{}", boundID, glm::to_string(s.min),
							   glm::to_string(s.max));
	}
	SetShardCount(bws.size());
}
void WatchDog::SwitchHeuristic(IHeuristic::Type newHeuristic)
{
	ActiveHeuristic = newHeuristic;
	switch (ActiveHeuristic)
	{
		case IHeuristic::Type::eNone:
			Heuristic = nullptr;
		case IHeuristic::Type::eGridCell:
		{
			Heuristic = std::make_shared<GridHeuristic>(GridHeuristic::Options{});
		}
		case IHeuristic::Type::eOctree:
		case IHeuristic::Type::eQuadtree:
			break;
	}
}
void WatchDog::HeuristicThreadEntry(std::stop_token st)
{
	ComputeHeuristic();
	while (!st.stop_requested())
	{
		std::this_thread::sleep_for(std::chrono::seconds(_WATCHDOG_COMPUTE_HEURISTIC_INTERVAL_S));
		ComputeHeuristic();
	}
}

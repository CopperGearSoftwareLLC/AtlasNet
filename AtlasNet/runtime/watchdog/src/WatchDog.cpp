#include "WatchDog.hpp"

#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <cstdlib>
#include <stop_token>
#include <thread>
#include <unordered_set>

#include "Entity/EntityHandoff/Telemetry/HandoffTransferManifest.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Docker/DockerIO.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityList.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Debug/LogEvent.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
constexpr auto kHandoffAuditInterval = std::chrono::milliseconds(250);
constexpr uint64_t kHandoffDiscrepancyThresholdUs = 1500000ULL;

std::unordered_set<std::string> GetLiveShardIds()
{
	std::unordered_map<std::string, double> pingByRawKey;
	HealthManifest::Get().GetAllPings(pingByRawKey);

	const double nowSec = InternalDB::Get()->GetTimeNowSeconds();
	std::unordered_set<std::string> live;
	live.reserve(pingByRawKey.size());
	for (const auto& [rawKey, expiresAtSec] : pingByRawKey)
	{
		if (expiresAtSec <= nowSec)
		{
			continue;
		}

		try
		{
			ByteReader br(rawKey);
			NetworkIdentity id;
			id.Deserialize(br);
			if (id.Type == NetworkIdentityType::eShard)
			{
				live.insert(id.ToString());
			}
		}
		catch (...)
		{
		}
	}
	return live;
}
}  // namespace

WatchDog::WatchDog() {}

WatchDog::~WatchDog() {}

void WatchDog::ClearAllDatabaseState()
{
	// Ensure cache is initialized

	logger->Debug(
		"Clearing all entity and shape data from database and memory cache");
}
void WatchDog::Run()
{
	Init();
	auto nextHandoffAudit = std::chrono::steady_clock::now() + kHandoffAuditInterval;
	while (!ShouldShutdown)
	{
		const auto now = std::chrono::steady_clock::now();
		if (now >= nextHandoffAudit)
		{
			AuditActiveHandoffTransfers();
			nextHandoffAudit = now + kHandoffAuditInterval;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	logger->Debug("Shutting down");
	Cleanup();
	Interlink::Get().Shutdown();
}
void WatchDog::Init()
{
	logger->Debug("Init");

	NetworkManifest::Get().ScheduleNetworkPings(ID);
	HealthManifest::Get().ScheduleHealthPings(ID);
	HealthManifest::Get().ScheduleHealthChecks(
		[&](const NetworkIdentity &ID_fail, const std::string &key)
		{
			logger->ErrorFormatted(
				"HEALTH CHECK FAIL : {}. Removing from Health Manifest",
				ID_fail.ToString());
			const bool requeued = HeuristicManifest::Get().RequeueClaimedBound(
				ID_fail.ToString());
			if (requeued)
			{
				logger->DebugFormatted("Requeued claimed bounds for {}",
									   ID_fail.ToString());
			}
			HealthManifest::Get().RemovePing(key);
		});
	InterlinkProperties InterLinkProps;
	InterLinkProps.logger = logger;
	InterLinkProps.ThisID = ID;
	Interlink::Get().Init(InterLinkProps);
	EventSystem::Get().Init(ID);

	SwitchHeuristic(IHeuristic::Type::eGridCell);
	ComputeHeuristic();	 // compute once
	// HeuristicThread = std::jthread([this](std::stop_token st)
	//							   { this->HeurristicThreadEntry(st); });
}

void WatchDog::Cleanup()
{
	Interlink::Get().Shutdown();
}

void WatchDog::SetShardCount(uint32 NewCount)
{
	const std::string filter =
		"%7B%22label%22%3A%5B%22atlasnet.role%3Dshard%22%5D%7D";

	std::string listResp =
		DockerIO::Get().request("GET", "/services?filters=" + filter);

	auto services = Json::parse(listResp);

	if (!services.is_array() || services.empty())
	{
		throw std::runtime_error("No shard service found via label");
	}

	// If you expect exactly one shard service:
	const std::string serviceId = services[0]["ID"];

	// 2️⃣ Inspect service by ID
	std::string inspectResp =
		DockerIO::Get().request("GET", "/services/" + serviceId);

	auto inspectJson = Json::parse(inspectResp);

	try
	{
		int version = inspectJson["Version"]["Index"];
		auto spec = inspectJson["Spec"];

		spec["Mode"]["Replicated"]["Replicas"] = NewCount;

		std::string updatePath = "/services/" + serviceId +
								 "/update?version=" + std::to_string(version);

		std::string updateResp =
			DockerIO::Get().request("POST", updatePath, &spec);

		if (!updateResp.empty())
		{
			logger->DebugFormatted("Service update responded with\n{}",
								   Json::parse(updateResp).dump(4));
		}
	}
	catch (const std::exception &e)
	{
		logger->ErrorFormatted("Inspect response:\n{}", inspectJson.dump(4));
		throw;
	}

	logger->DebugFormatted("Scaled shard service to {} replicas", NewCount);
	ShardCount = NewCount;
}
void WatchDog::ComputeHeuristic()
{
	AtlasEntityList<AtlasEntityMinimal> entities;
	for (int i = 0; i < 4; i++)
	{
		AtlasEntityMinimal e;
		e.transform.position = {10 * ((i / 2) ? 1 : -1),
								10 * ((i % 2) ? 1 : -1), 0};
		entities.push_back(e);
	}
	logger->DebugFormatted("Computing Heuristic: {}",
						   IHeuristic::TypeToString(ActiveHeuristic));

	HeuristicManifest::Get().SetActiveHeuristicType(ActiveHeuristic);
	logger->Debug("IHeuristic::Compute");
	Heuristic->Compute(entities.span());
	std::unordered_map<IBounds::BoundsID, ByteWriter> bws;
	logger->Debug("IHeuristic::SerializeBounds");
	Heuristic->SerializeBounds(bws);
	HeuristicManifest::Get().PushHeuristic(*Heuristic);
	logger->Debug("HeuristicManifest::GetPendingBoundsCount");
	const long long pending_count =
		HeuristicManifest::Get().GetPendingBoundsCount();
	logger->Debug("HeuristicManifest::GetClaimedBoundsCount");
	const long long claimed_count =
		HeuristicManifest::Get().GetClaimedBoundsCount();
	const long long total_known = pending_count + claimed_count;
	const long long expected = static_cast<long long>(bws.size());
	if (total_known < expected)
	{
		logger->Debug("HeuristicManifest::StorePendingBoundsFromByteWriters");
		HeuristicManifest::Get().StorePendingBoundsFromByteWriters(bws);
	}
	else
	{
		logger->DebugFormatted(
			"Skipping pending refresh (pending={}, claimed={}, expected={})",
			pending_count, claimed_count, expected);
	}

	std::vector<std::string> data;
	std::unordered_map<IBounds::BoundsID, ByteReader> brs;
	logger->Debug("HeuristicManifest::GetPendingBoundsAsByteReaders");
	HeuristicManifest::Get().GetPendingBoundsAsByteReaders(data, brs);

	for (auto &[boundID, bound_reader] : brs)
	{
		GridShape s;
		s.Deserialize(bound_reader);
		logger->DebugFormatted("Retreived ID {}, min:{}, max:{}", boundID,
							   glm::to_string(s.aabb.min),
							   glm::to_string(s.aabb.max));
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
			Heuristic = std::make_shared<GridHeuristic>();
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
		std::this_thread::sleep_for(
			std::chrono::seconds(_WATCHDOG_COMPUTE_HEURISTIC_INTERVAL_S));
		ComputeHeuristic();
	}
}

void WatchDog::AuditActiveHandoffTransfers()
{
	std::vector<HandoffTransferManifest::ActiveTransferRecord> activeTransfers;
	HandoffTransferManifest::Get().GetAllActiveTransfers(activeTransfers);
	if (activeTransfers.empty())
	{
		return;
	}

	const uint64_t nowUs = HandoffTransferManifest::Get().NowUnixTimeUs();
	const auto liveShardIds = GetLiveShardIds();
	for (const auto& transfer : activeTransfers)
	{
		if (transfer.transferTimeUs == 0)
		{
			continue;
		}
		if (nowUs <= transfer.transferTimeUs + kHandoffDiscrepancyThresholdUs)
		{
			continue;
		}

		const auto holders =
			HandoffTransferManifest::Get().GetTransferHolders(transfer.entityId);
		size_t liveHolderCount = 0;
		for (const auto& holder : holders)
		{
			if (liveShardIds.contains(holder))
			{
				++liveHolderCount;
			}
		}

		if (liveHolderCount == 1)
		{
			bool targetIsLiveHolder = false;
			bool sourceIsLiveHolder = false;
			for (const auto& holder : holders)
			{
				if (!liveShardIds.contains(holder))
				{
					continue;
				}
				if (holder == transfer.target)
				{
					targetIsLiveHolder = true;
				}
				if (holder == transfer.source)
				{
					sourceIsLiveHolder = true;
				}
			}

			if (targetIsLiveHolder)
			{
				HandoffTransferManifest::Get().ClearTransfer(transfer.entityId);
				continue;
			}
			if (sourceIsLiveHolder)
			{
				logger->WarningFormatted(
					"[EntityHandoff][WatchDog] STALLED_TRANSFER entity={} from={} to={} "
					"state={} overdue_ms={} holders={} live_holders=1(source)",
					transfer.entityId, transfer.source, transfer.target,
					transfer.state, (nowUs - transfer.transferTimeUs) / 1000ULL,
					holders.size());
			}
			continue;
		}

		const uint64_t overdueMs =
			(nowUs - transfer.transferTimeUs) / 1000ULL;
		if (liveHolderCount == 0)
		{
			logger->ErrorFormatted(
				"[EntityHandoff][WatchDog] LIMBO entity={} from={} to={} state={} "
				"overdue_ms={} holders={} live_holders=0",
				transfer.entityId, transfer.source, transfer.target,
				transfer.state, overdueMs, holders.size());
			continue;
		}

		logger->ErrorFormatted(
			"[EntityHandoff][WatchDog] DUAL_AUTH entity={} from={} to={} state={} "
			"overdue_ms={} holders={} live_holders={}",
			transfer.entityId, transfer.source, transfer.target, transfer.state,
			overdueMs, holders.size(), liveHolderCount);
	}
}

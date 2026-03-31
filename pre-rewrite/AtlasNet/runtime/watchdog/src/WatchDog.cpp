#include "WatchDog.hpp"

#include <curl/curl.h>

#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string_view>
#include <thread>
#include <unordered_set>

// #include "Entity/EntityHandoff/Telemetry/HandoffTransferManifest.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Transform.hpp"
#include "Events/Events/Debug/LogEvent.hpp"
#include "Events/GlobalEvents.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/HeuristicService.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Quadtree/QuadtreeHeuristic.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/LlmVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiHeuristic.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"
/*
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
}  // namespace */

namespace
{
std::string ReadTextFile(const char* path)
{
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file)
	{
		return {};
	}

	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void TrimTrailingWhitespace(std::string& value)
{
	while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
							  value.back() == '\t' || value.back() == ' '))
	{
		value.pop_back();
	}
}

size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	if (!userdata || !ptr)
	{
		return 0;
	}
	auto* out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}

std::string ResolvePodNamespace()
{
	if (const char* podNamespace = std::getenv("POD_NAMESPACE"); podNamespace && *podNamespace)
	{
		return std::string(podNamespace);
	}

	std::string namespaceFromFile =
		ReadTextFile("/var/run/secrets/kubernetes.io/serviceaccount/namespace");
	TrimTrailingWhitespace(namespaceFromFile);
	return namespaceFromFile;
}

bool ScaleK8sShardDeployment(const std::shared_ptr<Log>& logger, const uint32_t replicaCount)
{
	const char* host = std::getenv("KUBERNETES_SERVICE_HOST");
	if (!host || !*host)
	{
		return false;
	}

	const std::string port = [&]() -> std::string
	{
		const char* envPort = std::getenv("KUBERNETES_SERVICE_PORT_HTTPS");
		if (envPort && *envPort)
		{
			return std::string(envPort);
		}
		return "443";
	}();

	const std::string tokenPath = "/var/run/secrets/kubernetes.io/serviceaccount/token";
	std::string token = ReadTextFile(tokenPath.c_str());
	TrimTrailingWhitespace(token);
	if (token.empty())
	{
		logger->Error("Kubernetes service account token is missing.");
		return false;
	}

	const std::string podNamespace = ResolvePodNamespace();
	if (podNamespace.empty())
	{
		logger->Error("Unable to resolve pod namespace for Kubernetes scaling.");
		return false;
	}

	const std::string deploymentName = [&]() -> std::string
	{
		const char* envDeployment = std::getenv("ATLASNET_SHARD_DEPLOYMENT");
		if (envDeployment && *envDeployment)
		{
			return std::string(envDeployment);
		}
		return "atlasnet-shard";
	}();

	const std::string requestUrl =
		std::format("https://{}:{}/apis/apps/v1/namespaces/{}/deployments/{}/scale", host, port,
					podNamespace, deploymentName);

	Json payload;
	payload["spec"]["replicas"] = replicaCount;
	const std::string payloadText = payload.dump();

	static const bool curlInitialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
	if (!curlInitialized)
	{
		logger->Error("libcurl initialization failed for Kubernetes scaling.");
		return false;
	}

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		logger->Error("Failed to initialize CURL handle for Kubernetes scaling.");
		return false;
	}

	std::string responseBody;
	struct curl_slist* headers = nullptr;
	const std::string authHeader = "Authorization: Bearer " + token;
	headers = curl_slist_append(headers, authHeader.c_str());
	headers = curl_slist_append(headers, "Content-Type: application/merge-patch+json");

	const std::string caPath = "/var/run/secrets/kubernetes.io/serviceaccount/ca.crt";
	curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadText.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payloadText.size()));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, caPath.c_str());

	const CURLcode curlCode = curl_easy_perform(curl);
	long responseCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (curlCode != CURLE_OK)
	{
		logger->ErrorFormatted("Kubernetes scale request failed: {}", curl_easy_strerror(curlCode));
		return false;
	}

	if (responseCode < 200 || responseCode >= 300)
	{
		logger->ErrorFormatted("Kubernetes scale request returned HTTP {}. Body: {}", responseCode,
							   responseBody);
		return false;
	}

	return true;
}
}  // namespace

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
	// auto nextHandoffAudit = std::chrono::steady_clock::now() + kHandoffAuditInterval;
	while (!ShouldShutdown)
	{ /*
		 const auto now = std::chrono::steady_clock::now();
		 if (now >= nextHandoffAudit)
		 {
			 AuditActiveHandoffTransfers();
			 nextHandoffAudit = now + kHandoffAuditInterval;
		 } */
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	logger->Debug("Shutting down");
	Cleanup();
}
void WatchDog::Init()
{
	logger->Debug("Init");
	NetworkCredentials::Make(NetworkIdentity::MakeIDWatchDog());
	NetworkManifest::Get().ScheduleNetworkPings();
	HealthManifest::Get().ScheduleHealthPings();
	HealthManifest::Get().ScheduleHealthChecks(
		[&](const NetworkIdentity& ID_fail, const std::string& key)
		{
			logger->ErrorFormatted("HEALTH CHECK FAIL : {}. Removing from Health Manifest",
								   ID_fail.ToString());

			const bool requeued = HeuristicManifest::Get().ReleaseClaimedBound(ID_fail);
			if (requeued)
			{
				logger->DebugFormatted("Requeued claimed bounds for {}", ID_fail.ToString());
			}
			HealthManifest::Get().RemovePing(key);
			if (ID_fail.IsInternal() && ID_fail != NetworkCredentials::Get().GetID())
			{
				NetworkManifest::Get().RemoveTelemetry(ID_fail);
				ServerRegistry::Get().DeRegisterSelf(ID_fail);
			}
		});
	Interlink::Get().Init();
	GlobalEvents::Get().Init();
	HeuristicService::Ensure();

	// HeuristicService is the active recompute/publish path for heuristic data.
	// The local WatchDog heuristic instance remains available for ad-hoc
	// debugging and testing, but it is not the live runtime publisher while
	// WatchDog::ComputeHeuristic() stays disabled.
	SwitchHeuristic(IHeuristic::Type::eHotspotVoronoi);
	ComputeHeuristic();	 // compute once
						 // HeuristicThread = std::jthread([this](std::stop_token st)
						 //							   { this->HeurristicThreadEntry(st); });
}

void WatchDog::Cleanup()
{
	Interlink::Get().Shutdown();
}

void WatchDog::SetShardCount(uint32 NewCount) {}
void WatchDog::ComputeHeuristic()
{
	/*
	HeuristicManifest::Get().PushHeuristic(*Heuristic);
	std::unordered_map<BoundsID, ByteWriter> bws;
	logger->Debug("IHeuristic::SerializeBounds");
	Heuristic->SerializeBounds(bws);
	logger->Debug("HeuristicManifest::GetPendingBoundsCount");
	const long long pending_count = HeuristicManifest::Get().GetPendingBoundsCount();
	logger->Debug("HeuristicManifest::GetClaimedBoundsCount");
	const long long claimed_count = HeuristicManifest::Get().GetClaimedBoundsCount();
	const long long total_known = pending_count + claimed_count;
	const long long expected = static_cast<long long>(bws.size());
	if (total_known < expected)
	{
		logger->Debug("HeuristicManifest::StorePendingBoundsFromByteWriters");
		HeuristicManifest::Get().StorePendingBoundsFromByteWriters(bws);
	}
	else
	{
		logger->DebugFormatted("Skipping pending refresh (pending={}, claimed={}, expected={})",
							   pending_count, claimed_count, expected);
	}

	std::vector<std::string> data;
	std::unordered_map<BoundsID, ByteReader> brs;
	logger->Debug("HeuristicManifest::GetPendingBoundsAsByteReaders");
	HeuristicManifest::Get().GetPendingBoundsAsByteReaders(data, brs);

	for (auto& [boundID, bound_reader] : brs)
	{
		GridShape s;
		s.Deserialize(bound_reader);
		logger->DebugFormatted("Retreived ID {}, min:{}, max:{}", boundID,
							   glm::to_string(s.aabb.min), glm::to_string(s.aabb.max));
	}
	SetShardCount(bws.size());*/
}
void WatchDog::SwitchHeuristic(IHeuristic::Type newHeuristic)
{
	ActiveHeuristic = newHeuristic;
	switch (ActiveHeuristic)
	{
		case IHeuristic::Type::eNone:
			Heuristic = nullptr;
			break;
		case IHeuristic::Type::eInvalid:
			Heuristic = nullptr;
			break;
		case IHeuristic::Type::eGridCell:
		{
			Heuristic = std::make_shared<GridHeuristic>();
			break;
		}
		case IHeuristic::Type::eQuadtree:
		{
			Heuristic = std::make_shared<QuadtreeHeuristic>();
			break;
		}
		case IHeuristic::Type::eVoronoi:
		{
			Heuristic = std::make_shared<VoronoiHeuristic>();
			break;
		}
		case IHeuristic::Type::eHotspotVoronoi:
		{
			Heuristic = std::make_shared<HotspotVoronoiHeuristic>();
			break;
		}
		case IHeuristic::Type::eLlmVoronoi:
		{
			Heuristic = std::make_shared<LlmVoronoiHeuristic>();
			break;
		}
		case IHeuristic::Type::eOctree:
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

/* void WatchDog::AuditActiveHandoffTransfers()
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
} */

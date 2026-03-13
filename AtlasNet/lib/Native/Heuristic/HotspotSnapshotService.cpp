#include "HotspotSnapshotService.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Network/NetworkEnums.hpp"
#include "Snapshot/SnapshotService.hpp"

namespace
{
constexpr std::string_view kRecomputeSnapshotKey = "Heuristic:RecomputeSnapshots";
constexpr size_t kMaxSnapshotHistory = 64;
constexpr size_t kGridResolution = 24;
constexpr size_t kTargetHotspotCount = 5;
constexpr std::string_view kInputSchemaName = "seed_generation_v1";
constexpr float kWorldMinX = -100.0f;
constexpr float kWorldMaxX = 100.0f;
constexpr float kWorldMinY = -100.0f;
constexpr float kWorldMaxY = 100.0f;
constexpr double kAspectRatio = 1.0;

struct NormalizedPoint
{
	double x = 0.0;
	double y = 0.0;
};

struct PeakCandidate
{
	size_t cellX = 0;
	size_t cellY = 0;
	double score = 0.0;
	double x = 0.0;
	double y = 0.0;
};

struct Hotspot
{
	double x = 0.0;
	double y = 0.0;
	double weight = 0.0;
	double radius = 0.0;
};

struct RecomputeContext
{
	std::vector<Transform> transforms;
	std::vector<NormalizedPoint> normalizedPoints;
	std::vector<Hotspot> hotspots;
	uint32_t availableServers = 0;
	std::string targetHeuristicType;
	int64_t createdAtMs = 0;
	uint64_t cycleId = 0;
};

struct RecomputeSnapshotRecord
{
	std::string recomputeHeuristic;
	std::string inputSchema;
	Json diagnostics = Json::object();
	Json inputJson = Json::object();
};

[[nodiscard]] Json ToHotspotJson(const Hotspot& hotspot)
{
	return Json{
		{"x", hotspot.x},
		{"y", hotspot.y},
		{"weight", hotspot.weight},
		{"radius", hotspot.radius},
	};
}

[[nodiscard]] double Clamp01(const double value)
{
	return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double NormalizeCoordinate(
	const float value, const float minValue, const float maxValue)
{
	const double range =
		static_cast<double>(maxValue) - static_cast<double>(minValue);
	if (range <= 0.0)
	{
		return 0.5;
	}
	return Clamp01(
		(static_cast<double>(value) - static_cast<double>(minValue)) / range);
}

[[nodiscard]] size_t ToGridIndex(const double normalized)
{
	const double scaled = Clamp01(normalized) * static_cast<double>(kGridResolution);
	const auto idx = static_cast<size_t>(scaled);
	return std::min(idx, kGridResolution - 1);
}

[[nodiscard]] Json MakeEmptySnapshotDocument()
{
	Json doc = Json::object();
	doc["version"] = 1;
	doc["latestSnapshotId"] = 0;
	doc["latestCycleId"] = 0;
	doc["latestUpdatedMs"] = 0;
	doc["snapshots"] = Json::array();
	return doc;
}

[[nodiscard]] std::optional<std::string> ReadJsonValue(const std::string_view key)
{
	return InternalDB::Get()->WithSync(
		[&](auto& redis) -> std::optional<std::string>
		{
			const std::array<std::string, 3> cmd = {
				"JSON.GET",
				std::string(key),
				".",
			};
			return redis.template command<std::optional<std::string>>(
				cmd.begin(), cmd.end());
		});
}

void WriteJsonValue(const std::string_view key, const Json& value)
{
	(void)InternalDB::Get()->WithSync(
		[&](auto& redis)
		{
			const std::array<std::string, 4> cmd = {
				"JSON.SET",
				std::string(key),
				".",
				value.dump(),
			};
			return redis.command(cmd.begin(), cmd.end());
		});
}

[[nodiscard]] Json LoadSnapshotDocument()
{
	const std::optional<std::string> payload = ReadJsonValue(kRecomputeSnapshotKey);
	if (!payload.has_value() || payload->empty() || *payload == "null")
	{
		return MakeEmptySnapshotDocument();
	}

	Json doc = Json::parse(*payload, nullptr, false);
	if (doc.is_discarded() || !doc.is_object())
	{
		return MakeEmptySnapshotDocument();
	}

	if (!doc.contains("snapshots") || !doc["snapshots"].is_array())
	{
		doc["snapshots"] = Json::array();
	}
	if (!doc.contains("version"))
	{
		doc["version"] = 1;
	}
	if (!doc.contains("latestSnapshotId"))
	{
		doc["latestSnapshotId"] = 0;
	}
	if (!doc.contains("latestCycleId"))
	{
		doc["latestCycleId"] = 0;
	}
	if (!doc.contains("latestUpdatedMs"))
	{
		doc["latestUpdatedMs"] = 0;
	}
	return doc;
}

[[nodiscard]] int64_t NowUnixTimeMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::system_clock::now().time_since_epoch())
		.count();
}

[[nodiscard]] uint32_t ResolveAvailableServerCount()
{
	uint32_t shardCount = 0;
	for (const auto& [id, _entry] : ServerRegistry::Get().GetServers())
	{
		if (id.Type == NetworkIdentityType::eShard)
		{
			++shardCount;
		}
	}

	if (shardCount > 0)
	{
		return shardCount;
	}

	try
	{
		return HeuristicManifest::Get().PullHeuristic(
			[](const IHeuristic& heuristic) { return heuristic.GetBoundsCount(); });
	}
	catch (...)
	{
		return 0;
	}
}

[[nodiscard]] std::string ResolveTargetHeuristicType()
{
	try
	{
		return HeuristicManifest::Get().PullHeuristic(
			[](const IHeuristic& heuristic)
			{
				return std::string(IHeuristic::TypeToString(heuristic.GetType()));
			});
	}
	catch (...)
	{
		return "Unknown";
	}
}

[[nodiscard]] Json BuildHotspotInputJson(const RecomputeContext& context)
{
	Json inputJson = Json::object();
	inputJson["aspect_ratio"] = kAspectRatio;
	inputJson["k"] = context.availableServers;
	inputJson["hotspot_target_count"] = kTargetHotspotCount;
	inputJson["hotspots"] = Json::array();
	for (const Hotspot& hotspot : context.hotspots)
	{
		inputJson["hotspots"].push_back(ToHotspotJson(hotspot));
	}
	return inputJson;
}

[[nodiscard]] std::vector<RecomputeSnapshotRecord> BuildSnapshotRecords(
	const RecomputeContext& context)
{
	RecomputeSnapshotRecord hotspotRecord;
	hotspotRecord.recomputeHeuristic = "entity_hotspots_v1";
	hotspotRecord.inputSchema = std::string(kInputSchemaName);
	hotspotRecord.diagnostics = Json{
		{"entityCount", context.transforms.size()},
		{"availableServerCount", context.availableServers},
		{"hotspotTargetCount", kTargetHotspotCount},
		{"hotspotCount", context.hotspots.size()},
	};
	hotspotRecord.inputJson = BuildHotspotInputJson(context);

	std::vector<RecomputeSnapshotRecord> records;
	records.push_back(std::move(hotspotRecord));
	return records;
}

[[nodiscard]] std::vector<NormalizedPoint> NormalizeTransforms(
	const std::vector<Transform>& transforms)
{
	std::vector<NormalizedPoint> points;
	points.reserve(transforms.size());
	for (const Transform& transform : transforms)
	{
		points.push_back({
			.x = NormalizeCoordinate(transform.position.x, kWorldMinX, kWorldMaxX),
			.y = NormalizeCoordinate(transform.position.y, kWorldMinY, kWorldMaxY),
		});
	}
	return points;
}

[[nodiscard]] std::vector<PeakCandidate> FindPeakCandidates(
	const std::vector<NormalizedPoint>& points)
{
	if (points.empty())
	{
		return {};
	}

	std::array<double, kGridResolution * kGridResolution> counts{};
	for (const auto& point : points)
	{
		const size_t x = ToGridIndex(point.x);
		const size_t y = ToGridIndex(point.y);
		counts[y * kGridResolution + x] += 1.0;
	}

	std::array<double, kGridResolution * kGridResolution> smoothed{};
	double maxScore = 0.0;
	const std::array<std::array<int, 3>, 3> kernel = {{
		{{1, 2, 1}},
		{{2, 4, 2}},
		{{1, 2, 1}},
	}};

	for (size_t y = 0; y < kGridResolution; ++y)
	{
		for (size_t x = 0; x < kGridResolution; ++x)
		{
			double score = 0.0;
			for (int ky = -1; ky <= 1; ++ky)
			{
				for (int kx = -1; kx <= 1; ++kx)
				{
					const int nx = static_cast<int>(x) + kx;
					const int ny = static_cast<int>(y) + ky;
					if (nx < 0 || ny < 0 ||
						nx >= static_cast<int>(kGridResolution) ||
						ny >= static_cast<int>(kGridResolution))
					{
						continue;
					}
					score += counts[static_cast<size_t>(ny) * kGridResolution +
									static_cast<size_t>(nx)] *
							 static_cast<double>(
								 kernel[static_cast<size_t>(ky + 1)]
									   [static_cast<size_t>(kx + 1)]);
				}
			}
			score /= 16.0;
			smoothed[y * kGridResolution + x] = score;
			maxScore = std::max(maxScore, score);
		}
	}

	if (maxScore <= 0.0)
	{
		return {};
	}

	const double minPeakScore = std::max(1.0, maxScore * 0.15);
	std::vector<PeakCandidate> candidates;
	candidates.reserve(kTargetHotspotCount * 2);

	for (size_t y = 0; y < kGridResolution; ++y)
	{
		for (size_t x = 0; x < kGridResolution; ++x)
		{
			const double score = smoothed[y * kGridResolution + x];
			if (score < minPeakScore)
			{
				continue;
			}

			bool isLocalPeak = true;
			for (int ny = -1; ny <= 1 && isLocalPeak; ++ny)
			{
				for (int nx = -1; nx <= 1; ++nx)
				{
					if (nx == 0 && ny == 0)
					{
						continue;
					}

					const int px = static_cast<int>(x) + nx;
					const int py = static_cast<int>(y) + ny;
					if (px < 0 || py < 0 ||
						px >= static_cast<int>(kGridResolution) ||
						py >= static_cast<int>(kGridResolution))
					{
						continue;
					}

					if (smoothed[static_cast<size_t>(py) * kGridResolution +
								 static_cast<size_t>(px)] > score)
					{
						isLocalPeak = false;
						break;
					}
				}
			}

			if (!isLocalPeak)
			{
				continue;
			}

			candidates.push_back({
				.cellX = x,
				.cellY = y,
				.score = score,
				.x = (static_cast<double>(x) + 0.5) /
					 static_cast<double>(kGridResolution),
				.y = (static_cast<double>(y) + 0.5) /
					 static_cast<double>(kGridResolution),
			});
		}
	}

	std::sort(candidates.begin(), candidates.end(),
			  [](const PeakCandidate& left, const PeakCandidate& right)
			  { return left.score > right.score; });

	std::vector<PeakCandidate> deduped;
	deduped.reserve(std::min(candidates.size(), kTargetHotspotCount));
	const double minPeakDistance = 2.0 / static_cast<double>(kGridResolution);
	const double minPeakDistanceSq = minPeakDistance * minPeakDistance;

	for (const PeakCandidate& candidate : candidates)
	{
		bool tooClose = false;
		for (const PeakCandidate& existing : deduped)
		{
			const double dx = candidate.x - existing.x;
			const double dy = candidate.y - existing.y;
			if ((dx * dx) + (dy * dy) < minPeakDistanceSq)
			{
				tooClose = true;
				break;
			}
		}
		if (tooClose)
		{
			continue;
		}
		deduped.push_back(candidate);
		if (deduped.size() >= kTargetHotspotCount)
		{
			break;
		}
	}

	return deduped;
}

[[nodiscard]] std::vector<Hotspot> BuildHotspots(
	const std::vector<NormalizedPoint>& points)
{
	if (points.empty())
	{
		return {};
	}

	std::vector<PeakCandidate> peaks = FindPeakCandidates(points);
	if (peaks.empty())
	{
		peaks.push_back({
			.cellX = kGridResolution / 2,
			.cellY = kGridResolution / 2,
			.score = static_cast<double>(points.size()),
			.x = 0.5,
			.y = 0.5,
		});
	}

	struct Aggregate
	{
		double sumX = 0.0;
		double sumY = 0.0;
		size_t count = 0;
		std::vector<NormalizedPoint> assigned;
	};

	std::vector<Aggregate> aggregates(peaks.size());
	for (const auto& point : points)
	{
		size_t nearestIndex = 0;
		double nearestDistanceSq = std::numeric_limits<double>::max();
		for (size_t i = 0; i < peaks.size(); ++i)
		{
			const double dx = point.x - peaks[i].x;
			const double dy = point.y - peaks[i].y;
			const double distSq = (dx * dx) + (dy * dy);
			if (distSq < nearestDistanceSq)
			{
				nearestDistanceSq = distSq;
				nearestIndex = i;
			}
		}

		Aggregate& aggregate = aggregates[nearestIndex];
		aggregate.sumX += point.x;
		aggregate.sumY += point.y;
		aggregate.count += 1;
		aggregate.assigned.push_back(point);
	}

	std::vector<Hotspot> hotspots;
	hotspots.reserve(aggregates.size());
	const double minRadius = 1.0 / static_cast<double>(kGridResolution);
	const double gridCellDiagonal =
		std::sqrt(2.0) / static_cast<double>(kGridResolution);

	for (const Aggregate& aggregate : aggregates)
	{
		if (aggregate.count == 0)
		{
			continue;
		}

		const double centerX =
			aggregate.sumX / static_cast<double>(aggregate.count);
		const double centerY =
			aggregate.sumY / static_cast<double>(aggregate.count);

		double maxDistance = 0.0;
		for (const auto& point : aggregate.assigned)
		{
			const double dx = point.x - centerX;
			const double dy = point.y - centerY;
			maxDistance = std::max(
				maxDistance, std::sqrt((dx * dx) + (dy * dy)));
		}

		hotspots.push_back({
			.x = Clamp01(centerX),
			.y = Clamp01(centerY),
			.weight = static_cast<double>(aggregate.count) /
					  static_cast<double>(points.size()),
			.radius = std::clamp(
				maxDistance + (gridCellDiagonal * 0.5), minRadius, 0.5),
		});
	}

	std::sort(hotspots.begin(), hotspots.end(),
			  [](const Hotspot& left, const Hotspot& right)
			  { return left.weight > right.weight; });

	return hotspots;
}
}  // namespace

HotspotSnapshotService::HotspotSnapshotService()
{
	computeThread = std::jthread(
		[this](std::stop_token st) { ComputeThreadLoop(st); });
}

void HotspotSnapshotService::ComputeThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_HEURISTIC_RECOMPUTE_INTERNAL_MS);
	while (!st.stop_requested())
	{
		const auto startedAt = steady_clock::now();
		ComputeAndStoreSnapshot();

		const auto elapsed =
			duration_cast<milliseconds>(steady_clock::now() - startedAt);
		if (elapsed < interval)
		{
			std::this_thread::sleep_for(interval - elapsed);
		}
	}
}

void HotspotSnapshotService::ComputeAndStoreSnapshot()
{
	RecomputeContext context;
	SnapshotService::Get().FetchAllTransforms(context.transforms);
	context.normalizedPoints = NormalizeTransforms(context.transforms);
	context.hotspots = BuildHotspots(context.normalizedPoints);
	context.availableServers = ResolveAvailableServerCount();
	context.targetHeuristicType = ResolveTargetHeuristicType();
	context.createdAtMs = NowUnixTimeMs();

	Json doc = LoadSnapshotDocument();
	Json& snapshots = doc["snapshots"];
	if (!snapshots.is_array())
	{
		snapshots = Json::array();
	}

	uint64_t nextSnapshotId = doc.value("latestSnapshotId", 0ULL);
	context.cycleId = doc.value("latestCycleId", 0ULL) + 1ULL;
	Json latestSnapshot = Json::object();
	const std::vector<RecomputeSnapshotRecord> records =
		BuildSnapshotRecords(context);

	for (const RecomputeSnapshotRecord& record : records)
	{
		const uint64_t snapshotId = ++nextSnapshotId;
		Json snapshot = Json::object();
		snapshot["snapshotId"] = snapshotId;
		snapshot["cycleId"] = context.cycleId;
		snapshot["createdAtMs"] = context.createdAtMs;
		snapshot["recomputeHeuristic"] = record.recomputeHeuristic;
		snapshot["targetHeuristicType"] = context.targetHeuristicType;
		snapshot["inputSchema"] = record.inputSchema;
		snapshot["entityCount"] = context.transforms.size();
		snapshot["availableServerCount"] = context.availableServers;
		snapshot["hotspotCount"] = context.hotspots.size();
		snapshot["diagnostics"] = record.diagnostics;
		snapshot["input_json"] = record.inputJson;
		snapshot["output_json"] = nullptr;

		snapshots.push_back(snapshot);
		latestSnapshot = snapshot;
	}

	if (snapshots.size() > kMaxSnapshotHistory)
	{
		const auto removeCount = snapshots.size() - kMaxSnapshotHistory;
		for (size_t i = 0; i < removeCount; ++i)
		{
			snapshots.erase(snapshots.begin());
		}
	}

	doc["latestSnapshotId"] = nextSnapshotId;
	doc["latestCycleId"] = context.cycleId;
	doc["latestUpdatedMs"] = context.createdAtMs;
	doc["latest"] = latestSnapshot;

	WriteJsonValue(kRecomputeSnapshotKey, doc);
	logger.DebugFormatted(
		"Stored hotspot snapshot cycle {} with {} entities, {} hotspot(s), k={}, variants={}",
		context.cycleId, context.transforms.size(), context.hotspots.size(),
		context.availableServers, records.size());
}

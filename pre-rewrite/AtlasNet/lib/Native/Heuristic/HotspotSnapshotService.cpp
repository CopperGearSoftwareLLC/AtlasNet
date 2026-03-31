#include "HotspotSnapshotService.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "InternalDB/InternalDB.hpp"

namespace
{
constexpr std::string_view kRecomputeSnapshotKey = "Heuristic:RecomputeSnapshots";
constexpr size_t kMaxSnapshotHistory = 64;
constexpr std::string_view kHotspotRecomputeHeuristicName = "hotspot_voronoi_v1";
constexpr std::string_view kLlmRecomputeHeuristicName = "llm_voronoi_v1";
constexpr std::string_view kLegacyRecomputeHeuristicName = "legacy_random_voronoi_v1";
constexpr std::string_view kInputSchemaName = "seed_generation_v1";

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

[[nodiscard]] std::optional<Json> ExtractJsonObject(const std::string& raw)
{
	size_t start = std::string::npos;
	int depth = 0;
	bool inString = false;
	bool escaped = false;

	for (size_t i = 0; i < raw.size(); ++i)
	{
		const char ch = raw[i];
		if (escaped)
		{
			escaped = false;
			continue;
		}
		if (ch == '\\')
		{
			escaped = true;
			continue;
		}
		if (ch == '"')
		{
			inString = !inString;
			continue;
		}
		if (inString)
		{
			continue;
		}
		if (ch == '{')
		{
			if (depth == 0)
			{
				start = i;
			}
			++depth;
		}
		else if (ch == '}')
		{
			--depth;
			if (depth == 0 && start != std::string::npos)
			{
				Json parsed = Json::parse(raw.substr(start, i - start + 1), nullptr, false);
				if (!parsed.is_discarded() && parsed.is_object())
				{
					return parsed;
				}
				start = std::string::npos;
			}
		}
	}

	return std::nullopt;
}

[[nodiscard]] int64_t NowUnixTimeMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   std::chrono::system_clock::now().time_since_epoch())
		.count();
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
	return std::clamp(
		(static_cast<double>(value) - static_cast<double>(minValue)) / range,
		0.0,
		1.0);
}

[[nodiscard]] Json BuildInputJson(const HotspotVoronoiHeuristic& heuristic)
{
	Json inputJson = Json::object();
	const float worldWidth = heuristic.options.NetHalfExtent.x * 2.0f;
	const float worldHeight = heuristic.options.NetHalfExtent.y * 2.0f;
	inputJson["aspect_ratio"] =
		worldHeight > 0.0f ? static_cast<double>(worldWidth / worldHeight) : 1.0;
	inputJson["k"] = heuristic.GetActiveServerCount();
	inputJson["hotspots"] = Json::array();

	for (const HotspotVoronoiSample& hotspot : heuristic.GetHotspots())
	{
		inputJson["hotspots"].push_back(Json{
			{"x", hotspot.x},
			{"y", hotspot.y},
			{"weight", hotspot.weight},
			{"radius", hotspot.radius},
		});
	}

	return inputJson;
}

[[nodiscard]] Json BuildOutputJson(const HotspotVoronoiHeuristic& heuristic)
{
	Json outputJson = Json::object();
	outputJson["seeds"] = Json::array();
	outputJson["cells"] = Json::array();

	const float minX = -heuristic.options.NetHalfExtent.x;
	const float maxX = heuristic.options.NetHalfExtent.x;
	const float minY = -heuristic.options.NetHalfExtent.y;
	const float maxY = heuristic.options.NetHalfExtent.y;

	for (const glm::vec2& seed : heuristic.GetSeeds())
	{
		outputJson["seeds"].push_back(Json{
			{"x", NormalizeCoordinate(seed.x, minX, maxX)},
			{"y", NormalizeCoordinate(seed.y, minY, maxY)},
		});
	}

	std::vector<VoronoiBounds> cells;
	heuristic.GetBounds(cells);
	for (const VoronoiBounds& cell : cells)
	{
		Json cellJson = Json::object();
		cellJson["id"] = cell.ID;
		cellJson["site"] = Json{
			{"x", cell.site.x},
			{"y", cell.site.y},
		};
		cellJson["halfPlanes"] = Json::array();
		for (const VoronoiHalfPlane& plane : cell.halfPlanes)
		{
			cellJson["halfPlanes"].push_back(Json{
				{"nx", plane.normal.x},
				{"ny", plane.normal.y},
				{"c", plane.c},
			});
		}
		outputJson["cells"].push_back(std::move(cellJson));
	}

	return outputJson;
}

[[nodiscard]] Json BuildInputJson(const LlmVoronoiHeuristic& heuristic)
{
	Json inputJson = Json::object();
	const float worldWidth = heuristic.options.NetHalfExtent.x * 2.0f;
	const float worldHeight = heuristic.options.NetHalfExtent.y * 2.0f;
	inputJson["prompt"] = heuristic.GetLastPrompt();
	inputJson["aspect_ratio"] =
		worldHeight > 0.0f ? static_cast<double>(worldWidth / worldHeight) : 1.0;
	inputJson["k"] = heuristic.GetActiveServerCount();
	inputJson["hotspots"] = Json::array();

	for (const HotspotVoronoiSample& hotspot : heuristic.GetHotspots())
	{
		inputJson["hotspots"].push_back(Json{
			{"x", hotspot.x},
			{"y", hotspot.y},
			{"weight", hotspot.weight},
			{"radius", hotspot.radius},
		});
	}

	return inputJson;
}

[[nodiscard]] Json BuildOutputJson(const LlmVoronoiHeuristic& heuristic)
{
	Json outputJson = Json::object();
	outputJson["model_completion_raw"] = heuristic.GetLastCompletionRaw();
	if (const std::optional<Json> parsedCompletion =
			ExtractJsonObject(heuristic.GetLastCompletionRaw());
		parsedCompletion.has_value())
	{
		outputJson["model_completion_json"] = *parsedCompletion;
	}
	outputJson["seeds"] = Json::array();
	outputJson["cells"] = Json::array();

	const float minX = -heuristic.options.NetHalfExtent.x;
	const float maxX = heuristic.options.NetHalfExtent.x;
	const float minY = -heuristic.options.NetHalfExtent.y;
	const float maxY = heuristic.options.NetHalfExtent.y;

	for (const glm::vec2& seed : heuristic.GetSeeds())
	{
		outputJson["seeds"].push_back(Json{
			{"x", NormalizeCoordinate(seed.x, minX, maxX)},
			{"y", NormalizeCoordinate(seed.y, minY, maxY)},
		});
	}

	std::vector<VoronoiBounds> cells;
	heuristic.GetBounds(cells);
	for (const VoronoiBounds& cell : cells)
	{
		Json cellJson = Json::object();
		cellJson["id"] = cell.ID;
		cellJson["site"] = Json{
			{"x", cell.site.x},
			{"y", cell.site.y},
		};
		cellJson["halfPlanes"] = Json::array();
		for (const VoronoiHalfPlane& plane : cell.halfPlanes)
		{
			cellJson["halfPlanes"].push_back(Json{
				{"nx", plane.normal.x},
				{"ny", plane.normal.y},
				{"c", plane.c},
			});
		}
		outputJson["cells"].push_back(std::move(cellJson));
	}

	return outputJson;
}

[[nodiscard]] Json BuildLegacyInputJson(
	const VoronoiHeuristic& heuristic, const uint32_t availableServerCount)
{
	Json inputJson = Json::object();
	const float worldWidth = heuristic.options.NetHalfExtent.x * 2.0f;
	const float worldHeight = heuristic.options.NetHalfExtent.y * 2.0f;
	const float minX = -heuristic.options.NetHalfExtent.x;
	const float maxX = heuristic.options.NetHalfExtent.x;
	const float minY = -heuristic.options.NetHalfExtent.y;
	const float maxY = heuristic.options.NetHalfExtent.y;

	inputJson["aspect_ratio"] =
		worldHeight > 0.0f ? static_cast<double>(worldWidth / worldHeight) : 1.0;
	inputJson["k"] = availableServerCount;
	inputJson["points"] = Json::array();
	for (const glm::vec2& seed : heuristic.GetSeeds())
	{
		inputJson["points"].push_back(Json{
			{"x", NormalizeCoordinate(seed.x, minX, maxX)},
			{"y", NormalizeCoordinate(seed.y, minY, maxY)},
		});
	}
	return inputJson;
}

[[nodiscard]] Json BuildLegacyOutputJson(const VoronoiHeuristic& heuristic)
{
	Json outputJson = Json::object();
	outputJson["seeds"] = Json::array();
	outputJson["cells"] = Json::array();

	const float minX = -heuristic.options.NetHalfExtent.x;
	const float maxX = heuristic.options.NetHalfExtent.x;
	const float minY = -heuristic.options.NetHalfExtent.y;
	const float maxY = heuristic.options.NetHalfExtent.y;

	for (const glm::vec2& seed : heuristic.GetSeeds())
	{
		outputJson["seeds"].push_back(Json{
			{"x", NormalizeCoordinate(seed.x, minX, maxX)},
			{"y", NormalizeCoordinate(seed.y, minY, maxY)},
		});
	}

	std::vector<VoronoiBounds> cells;
	heuristic.GetBounds(cells);
	for (const VoronoiBounds& cell : cells)
	{
		Json cellJson = Json::object();
		cellJson["id"] = cell.ID;
		cellJson["vertices"] = Json::array();
		for (const glm::vec2& vertex : cell.vertices)
		{
			cellJson["vertices"].push_back(Json{
				{"x", NormalizeCoordinate(vertex.x, minX, maxX)},
				{"y", NormalizeCoordinate(vertex.y, minY, maxY)},
			});
		}
		outputJson["cells"].push_back(std::move(cellJson));
	}

	return outputJson;
}

void AppendSnapshotRecord(Json& doc, Json snapshot)
{
	Json& snapshots = doc["snapshots"];
	if (!snapshots.is_array())
	{
		snapshots = Json::array();
	}

	snapshots.push_back(snapshot);
	if (snapshots.size() > kMaxSnapshotHistory)
	{
		const auto removeCount = snapshots.size() - kMaxSnapshotHistory;
		for (size_t i = 0; i < removeCount; ++i)
		{
			snapshots.erase(snapshots.begin());
		}
	}

	doc["latestSnapshotId"] = snapshot["snapshotId"];
	doc["latestCycleId"] = snapshot["cycleId"];
	doc["latestUpdatedMs"] = snapshot["createdAtMs"];
	doc["latest"] = std::move(snapshot);
}
}  // namespace

void HotspotSnapshotService::StoreSnapshot(
	const HotspotVoronoiHeuristic& heuristic, const size_t entityCount)
{
	Json doc = LoadSnapshotDocument();
	const int64_t createdAtMs = NowUnixTimeMs();
	const uint64_t snapshotId = doc.value("latestSnapshotId", 0ULL) + 1ULL;
	const uint64_t cycleId = doc.value("latestCycleId", 0ULL) + 1ULL;

	Json snapshot = Json::object();
	snapshot["snapshotId"] = snapshotId;
	snapshot["cycleId"] = cycleId;
	snapshot["createdAtMs"] = createdAtMs;
	snapshot["recomputeHeuristic"] = std::string(kHotspotRecomputeHeuristicName);
	snapshot["targetHeuristicType"] =
		std::string(IHeuristic::TypeToString(heuristic.GetType()));
	snapshot["inputSchema"] = std::string(kInputSchemaName);
	snapshot["entityCount"] = entityCount;
	snapshot["availableServerCount"] = heuristic.GetActiveServerCount();
	snapshot["hotspotCount"] = heuristic.GetHotspots().size();
	snapshot["diagnostics"] = Json{
		{"hotspotTargetCount", heuristic.GetHotspotCount()},
		{"seedCount", heuristic.GetSeeds().size()},
	};
	snapshot["input_json"] = BuildInputJson(heuristic);
	snapshot["output_json"] = BuildOutputJson(heuristic);

	AppendSnapshotRecord(doc, std::move(snapshot));
	WriteJsonValue(kRecomputeSnapshotKey, doc);
}

void HotspotSnapshotService::StoreSnapshot(
	const LlmVoronoiHeuristic& heuristic, const size_t entityCount)
{
	Json doc = LoadSnapshotDocument();
	const int64_t createdAtMs = NowUnixTimeMs();
	const uint64_t snapshotId = doc.value("latestSnapshotId", 0ULL) + 1ULL;
	const uint64_t cycleId = doc.value("latestCycleId", 0ULL) + 1ULL;

	Json snapshot = Json::object();
	snapshot["snapshotId"] = snapshotId;
	snapshot["cycleId"] = cycleId;
	snapshot["createdAtMs"] = createdAtMs;
	snapshot["recomputeHeuristic"] = std::string(kLlmRecomputeHeuristicName);
	snapshot["targetHeuristicType"] =
		std::string(IHeuristic::TypeToString(heuristic.GetType()));
	snapshot["inputSchema"] = std::string(kInputSchemaName);
	snapshot["entityCount"] = entityCount;
	snapshot["availableServerCount"] = heuristic.GetActiveServerCount();
	snapshot["hotspotCount"] = heuristic.GetHotspots().size();
	snapshot["diagnostics"] = Json{
		{"hotspotTargetCount", heuristic.GetHotspotCount()},
		{"seedCount", heuristic.GetSeeds().size()},
		{"seedSource", heuristic.GetInferenceSource()},
		{"inferenceNote", heuristic.GetLastInferenceNote()},
		{"endpoint", heuristic.GetLastEndpointUrl()},
		{"modelId", heuristic.GetLastModelId()},
	};
	snapshot["input_json"] = BuildInputJson(heuristic);
	snapshot["output_json"] = BuildOutputJson(heuristic);

	AppendSnapshotRecord(doc, std::move(snapshot));
	WriteJsonValue(kRecomputeSnapshotKey, doc);
}

void HotspotSnapshotService::StoreSnapshot(
	const VoronoiHeuristic& heuristic,
	const size_t entityCount,
	const uint32_t availableServerCount)
{
	Json doc = LoadSnapshotDocument();
	const int64_t createdAtMs = NowUnixTimeMs();
	const uint64_t snapshotId = doc.value("latestSnapshotId", 0ULL) + 1ULL;
	const uint64_t cycleId = doc.value("latestCycleId", 0ULL) + 1ULL;

	Json snapshot = Json::object();
	snapshot["snapshotId"] = snapshotId;
	snapshot["cycleId"] = cycleId;
	snapshot["createdAtMs"] = createdAtMs;
	snapshot["recomputeHeuristic"] = std::string(kLegacyRecomputeHeuristicName);
	snapshot["targetHeuristicType"] =
		std::string(IHeuristic::TypeToString(heuristic.GetType()));
	snapshot["inputSchema"] = std::string(kInputSchemaName);
	snapshot["entityCount"] = entityCount;
	snapshot["availableServerCount"] = availableServerCount;
	snapshot["hotspotCount"] = 0;
	snapshot["diagnostics"] = Json{
		{"seedCount", heuristic.GetSeeds().size()},
	};
	snapshot["input_json"] = BuildLegacyInputJson(heuristic, availableServerCount);
	snapshot["output_json"] = BuildLegacyOutputJson(heuristic);

	AppendSnapshotRecord(doc, std::move(snapshot));
	WriteJsonValue(kRecomputeSnapshotKey, doc);
}

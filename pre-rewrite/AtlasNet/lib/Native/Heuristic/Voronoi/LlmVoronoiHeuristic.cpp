#include "LlmVoronoiHeuristic.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
std::optional<std::string> GetNonEmptyEnv(const char* key)
{
	const char* value = std::getenv(key);
	if (!value || !*value)
	{
		return std::nullopt;
	}
	return std::string(value);
}

uint32_t GetEnvUint32(const char* key, const uint32_t fallback)
{
	const auto value = GetNonEmptyEnv(key);
	if (!value.has_value())
	{
		return fallback;
	}

	try
	{
		return std::max<uint32_t>(1, static_cast<uint32_t>(std::stoul(*value)));
	}
	catch (...)
	{
		return fallback;
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

std::string FormatPromptNumber(const double value)
{
	std::ostringstream out;
	out << std::setprecision(6) << value;
	return out.str();
}

Json BuildPromptInputJson(
	const std::vector<HotspotVoronoiSample>& hotspots,
	const uint32_t serverCount,
	const LlmVoronoiHeuristic::Options& options)
{
	Json inputJson = Json::object();
	const float worldWidth = options.NetHalfExtent.x * 2.0f;
	const float worldHeight = options.NetHalfExtent.y * 2.0f;
	inputJson["aspect_ratio"] =
		worldHeight > 0.0f ? static_cast<double>(worldWidth / worldHeight) : 1.0;
	inputJson["k"] = serverCount;
	inputJson["hotspots"] = Json::array();
	for (const HotspotVoronoiSample& hotspot : hotspots)
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

std::string BuildPrompt(const Json& inputJson)
{
	std::ostringstream inputJsonText;
	inputJsonText << "{ \"aspect_ratio\": "
				  << FormatPromptNumber(inputJson.value("aspect_ratio", 1.0))
				  << ", \"k\": " << inputJson.value("k", 0);
	inputJsonText << ", \"hotspots\": [ ";
	const Json hotspots = inputJson.contains("hotspots") && inputJson["hotspots"].is_array()
							  ? inputJson["hotspots"]
							  : Json::array();
	for (size_t i = 0; i < hotspots.size(); ++i)
	{
		const Json& hotspot = hotspots[i];
		if (i > 0)
		{
			inputJsonText << ", ";
		}
		inputJsonText << "{ \"x\": " << FormatPromptNumber(hotspot.value("x", 0.0))
					  << ", \"y\": " << FormatPromptNumber(hotspot.value("y", 0.0))
					  << ", \"weight\": "
					  << FormatPromptNumber(hotspot.value("weight", 0.0))
					  << ", \"radius\": "
					  << FormatPromptNumber(hotspot.value("radius", 0.0)) << " }";
	}
	inputJsonText << " ] }";

	return std::string(
			   "You are placing shard seed centers in a normalized 2D world.\n\n"
			   "Coordinates are in [0,1] x [0,1]. The world aspect ratio is width/height.\n\n"
			   "Goal: Place K shard centers that balance load from hotspots.\n\n"
			   "Hotspots represent player density and include:\n\n"
			   "x, y position\n"
			   "weight (importance)\n"
			   "radius (influence area)\n"
			   "Guidelines:\n\n"
			   "Keep shard centers within [0,1] bounds\n"
			   "Prefer placing centers near clusters of hotspots\n"
			   "Avoid placing centers too close together\n"
			   "Try to distribute centers so hotspot load is balanced\n"
			   "Return ONLY valid JSON.\n\n"
			   "OUTPUT FORMAT: { \"seeds\": [ {\"x\": float, \"y\": float} ] }\n\n"
			   "INPUT_JSON: ") +
		   inputJsonText.str();
}

std::optional<std::string> ResolveEndpointUrl()
{
	auto normalize = [](std::string endpoint) -> std::string
	{
		if (endpoint.ends_with("/engines/v1"))
		{
			endpoint += "/chat/completions";
		}
		return endpoint;
	};

	if (const auto explicitEndpoint = GetNonEmptyEnv("ATLASNET_LLM_ENDPOINT");
		explicitEndpoint.has_value())
	{
		return normalize(*explicitEndpoint);
	}

	const auto path = GetNonEmptyEnv("ATLASNET_LLM_ENDPOINT_PATH").value_or(
		GetNonEmptyEnv("ATLASNET_LLM_API_FORMAT").value_or("openai") == "openai"
			? "/v1/chat/completions"
			: "/completion");

	if (const auto dockerMode = GetNonEmptyEnv("ATLASNET_DOCKER_MODE");
		dockerMode.has_value() && *dockerMode != "0" && *dockerMode != "false")
	{
		const std::string host =
			GetNonEmptyEnv("ATLASNET_LLM_DOCKER_HOST").value_or("172.17.0.1");
		const auto port = GetNonEmptyEnv("ATLASNET_LLM_DOCKER_PORT").value_or("12434");
		return normalize(std::format("http://{}:{}{}", host, port, path));
	}

	const std::string host =
		GetNonEmptyEnv("ATLASNET_LLM_SERVICE_HOST").value_or("atlasnet-llm");
	const auto port = GetNonEmptyEnv("ATLASNET_LLM_SERVICE_PORT").value_or("8080");
	return normalize(std::format("http://{}:{}{}", host, port, path));
}

bool IsOpenAiCompatibleEndpoint(const std::string& endpoint)
{
	if (const auto explicitFormat = GetNonEmptyEnv("ATLASNET_LLM_API_FORMAT");
		explicitFormat.has_value())
	{
		return *explicitFormat == "openai";
	}
	return endpoint.find("/engines/v1/") != std::string::npos ||
		   endpoint.find("/chat/completions") != std::string::npos;
}

std::string ResolveModelId()
{
	return GetNonEmptyEnv("ATLASNET_LLM_MODEL_ID")
		.value_or("huggingface.co/dannys0n/qwen3-1.7b-seed_gen_voronoi:Q4_K_M");
}

struct EndpointExecutionResult
{
	bool requestSucceeded = false;
	std::optional<std::string> completionText;
	std::string rawBody;
	std::string endpoint;
	std::string error;
};

std::optional<std::string> ExtractCompletionText(const std::string& responseBody)
{
	auto extractText = [&](const Json& value) -> std::optional<std::string>
	{
		if (value.is_string())
		{
			return value.get<std::string>();
		}
		if (value.is_array())
		{
			std::string combined;
			for (const auto& item : value)
			{
				if (item.is_string())
				{
					combined += item.get<std::string>();
					continue;
				}
				if (item.is_object())
				{
					if (const auto textIt = item.find("text");
						textIt != item.end() && textIt->is_string())
					{
						combined += textIt->get<std::string>();
						continue;
					}
					if (const auto contentIt = item.find("content");
						contentIt != item.end() && contentIt->is_string())
					{
						combined += contentIt->get<std::string>();
						continue;
					}
				}
			}
			if (!combined.empty())
			{
				return combined;
			}
		}
		return std::nullopt;
	};

	Json parsed = Json::parse(responseBody, nullptr, false);
	if (parsed.is_discarded())
	{
		return std::nullopt;
	}

	if (parsed.is_object())
	{
		if (const auto contentIt = parsed.find("content");
			contentIt != parsed.end())
		{
			if (const auto text = extractText(*contentIt); text.has_value())
			{
				return text;
			}
		}
		if (const auto choicesIt = parsed.find("choices");
			choicesIt != parsed.end() && choicesIt->is_array() && !choicesIt->empty())
		{
			const Json& choice = choicesIt->front();
			if (const auto textIt = choice.find("text");
				textIt != choice.end() && textIt->is_string())
			{
				return textIt->get<std::string>();
			}
			if (const auto messageIt = choice.find("message");
				messageIt != choice.end() && messageIt->is_object())
			{
				if (const auto contentIt = messageIt->find("content");
					contentIt != messageIt->end())
				{
					if (const auto text = extractText(*contentIt); text.has_value())
					{
						return text;
					}
				}
			}
		}
		if (parsed.contains("seeds"))
		{
			return parsed.dump();
		}
	}

	return std::nullopt;
}

EndpointExecutionResult ExecuteLlamaEndpoint(const std::string& prompt)
{
	const auto endpoint = ResolveEndpointUrl();
	if (!endpoint.has_value())
	{
		return {.requestSucceeded = false,
				.completionText = std::nullopt,
				.rawBody = "",
				.endpoint = "",
				.error = "endpoint_not_configured"};
	}

	static const bool curlInitialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
	if (!curlInitialized)
	{
		return {.requestSucceeded = false,
				.completionText = std::nullopt,
				.rawBody = "",
				.endpoint = *endpoint,
				.error = "curl_global_init_failed"};
	}

	CURL* curl = curl_easy_init();
	if (!curl)
	{
		return {
			.requestSucceeded = false,
			.completionText = std::nullopt,
			.rawBody = "",
			.endpoint = *endpoint,
			.error = "curl_easy_init_failed",
		};
	}

	const uint32_t maxTokens = GetEnvUint32("ATLASNET_LLM_MAX_TOKENS", 256);
	const long timeoutMs =
		static_cast<long>(GetEnvUint32("ATLASNET_LLM_HTTP_TIMEOUT_MS", 60000));

	Json requestBody = Json::object();
	if (IsOpenAiCompatibleEndpoint(*endpoint))
	{
		requestBody["model"] = ResolveModelId();
		requestBody["messages"] = Json::array(
			{Json{{"role", "user"}, {"content", prompt}}});
		requestBody["temperature"] = 0.0;
		requestBody["stream"] = false;
		requestBody["max_tokens"] = maxTokens;
	}
	else
	{
		requestBody["prompt"] = prompt;
		requestBody["n_predict"] = maxTokens;
		requestBody["temperature"] = 0.0;
		requestBody["stream"] = false;
		requestBody["cache_prompt"] = true;
	}

	const std::string payload = requestBody.dump();
	std::string responseBody;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, endpoint->c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs);

	const CURLcode curlCode = curl_easy_perform(curl);
	long responseCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (curlCode != CURLE_OK || responseCode < 200 || responseCode >= 300)
	{
		return {
			.requestSucceeded = false,
			.completionText = std::nullopt,
			.rawBody = responseBody,
			.endpoint = *endpoint,
			.error = curlCode != CURLE_OK
						 ? std::format("curl_error:{}", curl_easy_strerror(curlCode))
						 : std::format("http_error:{}", responseCode),
		};
	}

	if (responseBody.empty())
	{
		return {
			.requestSucceeded = false,
			.completionText = std::nullopt,
			.rawBody = "",
			.endpoint = *endpoint,
			.error = "empty_response_body",
		};
	}

	return {
		.requestSucceeded = true,
		.completionText = ExtractCompletionText(responseBody),
		.rawBody = responseBody,
		.endpoint = *endpoint,
		.error = "",
	};
}

std::optional<Json> ExtractJsonObject(const std::string& raw)
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
				Json parsed = Json::parse(
					raw.substr(start, i - start + 1), nullptr, false);
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

double Clamp01(const double value)
{
	return std::clamp(value, 0.0, 1.0);
}

float DenormalizeCoordinate(
	const double value, const float minValue, const float maxValue)
{
	return static_cast<float>(
		static_cast<double>(minValue) +
		Clamp01(value) *
			(static_cast<double>(maxValue) - static_cast<double>(minValue)));
}

std::optional<std::vector<glm::vec2>> ParseSeedResponse(
	const std::string& raw,
	const uint32_t expectedCount,
	const LlmVoronoiHeuristic::Options& options)
{
	const std::optional<Json> parsed = ExtractJsonObject(raw);
	if (!parsed.has_value())
	{
		return std::nullopt;
	}

	const auto seedsIt = parsed->find("seeds");
	if (seedsIt == parsed->end() || !seedsIt->is_array())
	{
		return std::nullopt;
	}

	std::vector<glm::vec2> seeds;
	seeds.reserve(expectedCount);
	for (const auto& seedJson : *seedsIt)
	{
		if (!seedJson.is_object())
		{
			continue;
		}

		const double x = seedJson.value("x", std::numeric_limits<double>::quiet_NaN());
		const double y = seedJson.value("y", std::numeric_limits<double>::quiet_NaN());
		if (!std::isfinite(x) || !std::isfinite(y))
		{
			continue;
		}

		seeds.emplace_back(
			DenormalizeCoordinate(x, -options.NetHalfExtent.x, options.NetHalfExtent.x),
			DenormalizeCoordinate(y, -options.NetHalfExtent.y, options.NetHalfExtent.y));
		if (seeds.size() >= expectedCount)
		{
			break;
		}
	}

	if (seeds.empty())
	{
		return std::nullopt;
	}
	return seeds;
}

}  // namespace

LlmVoronoiHeuristic::LlmVoronoiHeuristic() = default;

void LlmVoronoiHeuristic::SetAvailableServerCount(uint32_t count)
{
	requestedServerCount = count;
}

void LlmVoronoiHeuristic::SetHotspotCount(uint32_t count)
{
	options.HotspotCount = std::max<uint32_t>(1, count);
}

uint32_t LlmVoronoiHeuristic::GetActiveServerCount() const
{
	return activeServerCount;
}

uint32_t LlmVoronoiHeuristic::GetHotspotCount() const
{
	return options.HotspotCount;
}

const std::vector<HotspotVoronoiSample>& LlmVoronoiHeuristic::GetHotspots() const
{
	return hotspots;
}

const std::vector<glm::vec2>& LlmVoronoiHeuristic::GetSeeds() const
{
	return seeds;
}

bool LlmVoronoiHeuristic::UsedLlamaCpp() const
{
	return usedLlamaCpp;
}

const std::string& LlmVoronoiHeuristic::GetInferenceSource() const
{
	return inferenceSource;
}

const std::string& LlmVoronoiHeuristic::GetLastInferenceNote() const
{
	return lastInferenceNote;
}

const std::string& LlmVoronoiHeuristic::GetLastEndpointUrl() const
{
	return lastEndpointUrl;
}

const std::string& LlmVoronoiHeuristic::GetLastModelId() const
{
	return lastModelId;
}

const std::string& LlmVoronoiHeuristic::GetLastCompletionRaw() const
{
	return lastCompletionRaw;
}

const std::string& LlmVoronoiHeuristic::GetLastPrompt() const
{
	return lastPrompt;
}

void LlmVoronoiHeuristic::Compute(const std::span<const AtlasTransform>& span)
{
	const std::vector<glm::vec2> previousSeeds = seeds;
	const std::vector<VoronoiBounds> previousCells = cells;

	activeServerCount =
		requestedServerCount > 0 ? requestedServerCount : options.DefaultServerCount;
	activeServerCount = std::max<uint32_t>(1, activeServerCount);

	hotspots = HotspotVoronoiHeuristic::BuildHotspotsFromEntities(span, options);
	const Json inputJson = BuildPromptInputJson(hotspots, activeServerCount, options);
	lastPrompt = BuildPrompt(inputJson);

	usedLlamaCpp = false;
	inferenceSource = "algorithmic_fallback";
	lastInferenceNote = "algorithmic_fallback";
	lastEndpointUrl.clear();
	lastModelId = ResolveModelId();
	lastCompletionRaw.clear();
	const auto resolvedEndpoint = ResolveEndpointUrl();
	const bool openAiEndpoint =
		resolvedEndpoint.has_value() && IsOpenAiCompatibleEndpoint(*resolvedEndpoint);

	const EndpointExecutionResult endpointResult =
		ExecuteLlamaEndpoint(lastPrompt);
	lastEndpointUrl = endpointResult.endpoint;
	lastCompletionRaw = endpointResult.completionText.value_or(endpointResult.rawBody);

	if (endpointResult.requestSucceeded)
	{
		std::optional<std::vector<glm::vec2>> parsedSeeds;
		if (endpointResult.completionText.has_value())
		{
			parsedSeeds =
				ParseSeedResponse(*endpointResult.completionText, activeServerCount, options);
		}
		if (!parsedSeeds.has_value() && !endpointResult.rawBody.empty())
		{
			parsedSeeds =
				ParseSeedResponse(endpointResult.rawBody, activeServerCount, options);
		}

		if (parsedSeeds.has_value())
		{
			if (parsedSeeds->size() == activeServerCount)
			{
				seeds = *parsedSeeds;
				cells = HotspotVoronoiHeuristic::BuildCellsFromSeeds(seeds, options);
				usedLlamaCpp = true;
				inferenceSource = openAiEndpoint ? "docker_model_runner" : "llama.cpp";
				lastInferenceNote =
					openAiEndpoint ? "docker_model_runner_endpoint" : "llama.cpp_endpoint";
			}
			else
			{
				if (!previousSeeds.empty() && previousCells.size() == previousSeeds.size())
				{
					seeds = previousSeeds;
					cells = previousCells;
					inferenceSource = "retained_previous";
					lastInferenceNote = std::format(
						"seed_count_mismatch_retained:{}_of_{}@{}",
						parsedSeeds->size(), activeServerCount,
						lastEndpointUrl.empty() ? "<none>" : lastEndpointUrl);
				}
				else
				{
					seeds = HotspotVoronoiHeuristic::GenerateAlgorithmicSeeds(
						hotspots, activeServerCount, options);
					cells = HotspotVoronoiHeuristic::BuildCellsFromSeeds(seeds, options);
					inferenceSource = "bootstrap_fallback";
					lastInferenceNote = std::format(
						"seed_count_mismatch_bootstrap:{}_of_{}@{}",
						parsedSeeds->size(), activeServerCount,
						lastEndpointUrl.empty() ? "<none>" : lastEndpointUrl);
				}
			}
		}
		else
		{
			if (!previousSeeds.empty() && previousCells.size() == previousSeeds.size())
			{
				seeds = previousSeeds;
				cells = previousCells;
				inferenceSource = "retained_previous";
				lastInferenceNote = std::format(
					"seed_parse_retained@{}",
					lastEndpointUrl.empty() ? "<none>" : lastEndpointUrl);
			}
			else
			{
				seeds = HotspotVoronoiHeuristic::GenerateAlgorithmicSeeds(
					hotspots, activeServerCount, options);
				cells = HotspotVoronoiHeuristic::BuildCellsFromSeeds(seeds, options);
				inferenceSource = "bootstrap_fallback";
				lastInferenceNote = std::format(
					"seed_parse_bootstrap@{}",
					lastEndpointUrl.empty() ? "<none>" : lastEndpointUrl);
			}
		}
	}
	else
	{
		lastInferenceNote = std::format(
			"endpoint_unavailable:{}@{}",
			endpointResult.error.empty() ? "unknown_error" : endpointResult.error,
			lastEndpointUrl.empty() ? "<none>" : lastEndpointUrl);
		seeds = HotspotVoronoiHeuristic::GenerateAlgorithmicSeeds(
			hotspots, activeServerCount, options);
		cells = HotspotVoronoiHeuristic::BuildCellsFromSeeds(seeds, options);
	}

	logger.DebugFormatted(
		"LlmVoronoiHeuristic::Compute: entities={} hotspots={} seeds={} bounds={} source={}",
		span.size(), hotspots.size(), seeds.size(), cells.size(), lastInferenceNote);
}

uint32_t LlmVoronoiHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(cells.size());
}

void LlmVoronoiHeuristic::GetBounds(std::vector<VoronoiBounds>& out_bounds) const
{
	out_bounds = cells;
}

void LlmVoronoiHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<VoronoiBounds>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type LlmVoronoiHeuristic::GetType() const
{
	return IHeuristic::Type::eLlmVoronoi;
}

void LlmVoronoiHeuristic::SerializeBounds(
	std::unordered_map<BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const VoronoiBounds& cell : cells)
	{
		auto [it, inserted] = bws.emplace(cell.ID, ByteWriter{});
		(void)inserted;
		cell.Serialize(it->second);
	}
}

void LlmVoronoiHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(cells.size());
	for (const auto& cell : cells)
	{
		cell.Serialize(bw);
	}
}

void LlmVoronoiHeuristic::Deserialize(ByteReader& br)
{
	const size_t cellCount = br.u64();
	cells.resize(cellCount);
	hotspots.clear();
	seeds.clear();
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].Deserialize(br);
	}
}

std::optional<BoundsID> LlmVoronoiHeuristic::QueryPosition(vec3 p) const
{
	for (const auto& cell : cells)
	{
		if (cell.Contains(p))
		{
			return cell.ID;
		}
	}
	return std::nullopt;
}

const IBounds& LlmVoronoiHeuristic::GetBound(BoundsID id) const
{
	for (const auto& cell : cells)
	{
		if (cell.ID == id)
		{
			return cell;
		}
	}
	throw std::runtime_error("Invalid ID");
}

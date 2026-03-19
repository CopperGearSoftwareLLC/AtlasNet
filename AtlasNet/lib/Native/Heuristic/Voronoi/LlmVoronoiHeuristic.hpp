#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"

class LlmVoronoiHeuristic : public THeuristic<VoronoiBounds>
{
   public:
	using Options = HotspotVoronoiHeuristic::Options;

	Options options;

	LlmVoronoiHeuristic();

	void SetAvailableServerCount(uint32_t count);
	void SetHotspotCount(uint32_t count);
	[[nodiscard]] uint32_t GetActiveServerCount() const;
	[[nodiscard]] uint32_t GetHotspotCount() const;
	[[nodiscard]] const std::vector<HotspotVoronoiSample>& GetHotspots() const;
	[[nodiscard]] const std::vector<glm::vec2>& GetSeeds() const;
	[[nodiscard]] bool UsedLlamaCpp() const;
	[[nodiscard]] const std::string& GetInferenceSource() const;
	[[nodiscard]] const std::string& GetLastInferenceNote() const;
	[[nodiscard]] const std::string& GetLastEndpointUrl() const;
	[[nodiscard]] const std::string& GetLastModelId() const;
	[[nodiscard]] const std::string& GetLastCompletionRaw() const;
	[[nodiscard]] const std::string& GetLastPrompt() const;

	void Compute(const std::span<const AtlasTransform>& span) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<VoronoiBounds>& out_bounds) const override;
	void GetBoundDeltas(
		std::vector<TBoundDelta<VoronoiBounds>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(std::unordered_map<BoundsID, ByteWriter>& bws) override;

	void Serialize(ByteWriter& bw) const override;
	void Deserialize(ByteReader& br) override;

	std::optional<BoundsID> QueryPosition(vec3 p) const override;
	const IBounds& GetBound(BoundsID id) const override;

   private:
	uint32_t requestedServerCount = 0;
	uint32_t activeServerCount = 0;
	bool usedLlamaCpp = false;
	std::string inferenceSource = "algorithmic_fallback";
	std::string lastInferenceNote;
	std::string lastEndpointUrl;
	std::string lastModelId;
	std::string lastCompletionRaw;
	std::string lastPrompt;
	std::vector<HotspotVoronoiSample> hotspots;
	std::vector<glm::vec2> seeds;
	std::vector<VoronoiBounds> cells;
};

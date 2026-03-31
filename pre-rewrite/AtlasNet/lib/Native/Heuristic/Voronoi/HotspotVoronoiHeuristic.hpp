#pragma once

#include <optional>
#include <vector>

#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"

struct HotspotVoronoiSample
{
	double x = 0.0;
	double y = 0.0;
	double weight = 0.0;
	double radius = 0.0;
};

class HotspotVoronoiHeuristic : public THeuristic<VoronoiBounds>
{
   public:
	struct Options
	{
		vec2 NetHalfExtent = {100.0f, 100.0f};
		uint32_t DefaultServerCount = 3;
		uint32_t HotspotCount = 8;
		uint32_t DensityGridResolution = 24;
	} options;

	HotspotVoronoiHeuristic();

	void SetAvailableServerCount(uint32_t count);
	void SetHotspotCount(uint32_t count);
	[[nodiscard]] static std::vector<HotspotVoronoiSample> BuildHotspotsFromEntities(
		const std::span<const AtlasTransform>& span, const Options& options);
	[[nodiscard]] static std::vector<glm::vec2> GenerateAlgorithmicSeeds(
		const std::vector<HotspotVoronoiSample>& hotspots,
		uint32_t serverCount,
		const Options& options);
	[[nodiscard]] static std::vector<VoronoiBounds> BuildCellsFromSeeds(
		const std::vector<glm::vec2>& seeds, const Options& options);
	[[nodiscard]] uint32_t GetActiveServerCount() const;
	[[nodiscard]] uint32_t GetHotspotCount() const;
	[[nodiscard]] const std::vector<HotspotVoronoiSample>& GetHotspots() const;
	[[nodiscard]] const std::vector<glm::vec2>& GetSeeds() const;

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
	std::vector<HotspotVoronoiSample> hotspots;
	std::vector<glm::vec2> seeds;
	std::vector<VoronoiBounds> cells;
};

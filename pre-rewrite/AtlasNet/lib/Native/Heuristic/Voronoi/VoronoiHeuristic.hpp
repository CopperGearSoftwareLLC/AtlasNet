#pragma once

#include <vector>
#include <optional>

#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"

// Voronoi heuristic used for testing.
// Partitions the plane into N cells using Voronoi half-plane constraints.
class VoronoiHeuristic : public THeuristic<VoronoiBounds>
{
   public:
	struct Options
	{
		// Half-extent used only to seed initial sites deterministically.
		vec2 NetHalfExtent = {100.0f, 100.0f};
		// Number of Voronoi seeds / regions.
		uint32_t SeedCount = 3;
	} options;

	VoronoiHeuristic();

	// Allows callers (e.g. WatchDog) to request a seed count.
	void SetSeedCount(uint32_t count);
	[[nodiscard]] const std::vector<glm::vec2>& GetSeeds() const;

	// IHeuristic interface
	void Compute(const std::span<const AtlasTransform>& span) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<VoronoiBounds>& out_bounds) const override;
	void GetBoundDeltas(
		std::vector<TBoundDelta<VoronoiBounds>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(
		std::unordered_map<BoundsID, ByteWriter>& bws) override;

	void Serialize(ByteWriter& bw) const override;
	void Deserialize(ByteReader& br) override;

	std::optional<BoundsID> QueryPosition(vec3 p) const override;
	const IBounds& GetBound(BoundsID id) const override;

   private:
	uint64_t _computeRevision = 0;
	std::vector<glm::vec2> _seeds;
	std::vector<VoronoiBounds> _cells;
};

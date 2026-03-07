#pragma once

#include <vector>
#include <optional>

#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"

// Voronoi heuristic used for testing.
// Partitions the world into N convex polygon regions using standard Voronoi
// half-plane clipping within a fixed bounding box.
class VoronoiHeuristic : public THeuristic<VoronoiBounds>
{
   public:
	struct Options
	{
		// Half-extent of the total region in X/Y, matching the legacy grid.
		vec2 NetHalfExtent = {100.0f, 100.0f};
		// Number of Voronoi seeds / regions.
		uint32_t SeedCount = 16;
	} options;

	VoronoiHeuristic();

	// Allows callers (e.g. WatchDog) to request a seed count.
	void SetSeedCount(uint32_t count);

	// IHeuristic interface
	void Compute(const std::span<const Transform>& span) override;
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
	std::vector<glm::vec2> _seeds;
	std::vector<VoronoiBounds> _cells;
};


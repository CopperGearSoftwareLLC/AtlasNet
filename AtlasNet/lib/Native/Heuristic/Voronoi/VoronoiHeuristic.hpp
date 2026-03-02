#pragma once

#include <vector>
#include <optional>

#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"

// Voronoi-like heuristic used for testing.
// Partitions the world into a random, but axis-aligned, tiling by
// constructing random stripe boundaries along X and Y within a fixed
// net area. The resulting bounds are still GridShape AABBs so they
// remain wire-compatible with existing tooling.
class VoronoiHeuristic : public THeuristic<GridShape>
{
   public:
	struct Options
	{
		// Half-extent of the total region in X/Y, matching the legacy grid.
		vec2 NetHalfExtent = {100.0f, 100.0f};
		// Desired number of regions. For now this is assumed to be a
		// perfect square (side*side).
		uint32_t TargetCellCount = 16;
	} options;

	VoronoiHeuristic();

	// Allows callers (e.g. WatchDog) to request a specific number of cells.
	void SetTargetCellCount(uint32_t count);

	// IHeuristic interface
	void Compute(const std::span<const AtlasEntityMinimal>& span) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<GridShape>& out_bounds) const override;
	void GetBoundDeltas(
		std::vector<TBoundDelta<GridShape>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(
		std::unordered_map<IBounds::BoundsID, ByteWriter>& bws) override;

	void Serialize(ByteWriter& bw) const override;
	void Deserialize(ByteReader& br) override;

	std::optional<IBounds::BoundsID> QueryPosition(vec3 p) override;
	std::unique_ptr<IBounds> GetBound(IBounds::BoundsID id) override;

   private:
	std::vector<GridShape> _cells;
};


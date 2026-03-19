#pragma once

#include <vector>
#include <optional>

#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"

// Quadtree-backed heuristic that produces a fixed number of rectangular
// bounds (cells) covering a net area equivalent to the legacy grid heuristic.
// For now this is configured to a fixed grid of N x N cells, where
// N^2 == TargetLeafCount (rounded to the nearest square).
class QuadtreeHeuristic : public THeuristic<GridShape>
{
   public:
	struct Options
	{
		// Half-extent of the total region in X/Y.
		// The legacy grid heuristic produced 4 cells covering [-100, 100] in X/Y,
		// so we default to the same net area here.
		vec2 NetHalfExtent = {100.0f, 100.0f};

		// Desired number of leaf cells. WatchDog will currently set this to 16.
		uint32_t TargetLeafCount = 16;
	} options;

	QuadtreeHeuristic();

	// Allows callers (e.g. WatchDog) to request a specific number of cells.
	void SetTargetLeafCount(uint32_t count);

	// IHeuristic interface
	void Compute(const std::span<const AtlasTransform>& span) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<GridShape>& out_bounds) const override;
	void GetBoundDeltas(
		std::vector<TBoundDelta<GridShape>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(
		std::unordered_map<BoundsID, ByteWriter>& bws) override;

	void Serialize(ByteWriter& bw) const override;
	void Deserialize(ByteReader& br) override;

	std::optional<BoundsID> QueryPosition(vec3 p) const override;
	const IBounds& GetBound(BoundsID id) const override;

   private:
	std::vector<GridShape> _cells;
};


#pragma once

#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Types/AABB.hpp"
#include "pch.hpp"

struct GridShape : public IBounds, AABB3f
{
	void Serialize(ByteWriter& bw) const override
	{
		IBounds::Serialize(bw);
		AABB3f::Serialize(bw);
	}
	void Deserialize(ByteReader& br) override
	{
		IBounds::Deserialize(br);
		AABB3f::Deserialize(br);
	}
	bool Contains(vec3 p) const override { return AABB3f::contains(p); }
};
class GridHeuristic : public THeuristic<GridShape>
{
   public:
	struct Options
	{
		vec2 GridSize = {100, 100};
	} options;
	GridHeuristic(const Options& _options);

	void Compute(const AtlasEntitySpan<const AtlasEntityMinimal>&) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<GridShape>& out_bounds) const override;
	void GetBoundDeltas(std::vector<TBoundDelta<GridShape>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter>& bws) override;
};
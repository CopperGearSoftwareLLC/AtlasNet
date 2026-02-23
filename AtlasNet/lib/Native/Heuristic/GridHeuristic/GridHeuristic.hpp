#pragma once

#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"

struct GridShape : public IBounds
{
	AABB3f aabb;
	void Internal_SerializeData(ByteWriter& bw) const override { aabb.Serialize(bw); }
	void Internal_DeserializeData(ByteReader& br) override { aabb.Deserialize(br); }
	bool Contains(vec3 p) const override { return aabb.contains(p); }
	vec3 GetCenter() const override { return aabb.center(); }
};
class GridHeuristic : public THeuristic<GridShape>
{
   public:
	struct Options
	{
		vec2 GridSize = {100, 100};
	} options;
	GridHeuristic();
	std::vector<GridShape> quads;

	void Compute(const std::span<const AtlasEntityMinimal>&) override;
	uint32_t GetBoundsCount() const override;
	void GetBounds(std::vector<GridShape>& out_bounds) const override;
	void GetBoundDeltas(std::vector<TBoundDelta<GridShape>>& out_deltas) const override;
	IHeuristic::Type GetType() const override;
	void SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter>& bws) override;

	void Serialize(ByteWriter& bw) const override;
	void Deserialize(ByteReader& br) override;

	std::unique_ptr<IBounds> QueryPosition(vec3 p) override;
	std::span<const GridShape> GetGrids() const { return quads; }
};
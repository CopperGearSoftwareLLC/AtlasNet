#include "GridHeuristic.hpp"

#include <iostream>

#include "Global/Serialize/ByteWriter.hpp"
GridHeuristic::GridHeuristic() {}
void GridHeuristic::Compute(
	const std::span<const AtlasEntityMinimal>& span)
{
	quads.resize(4);
	for (int i = 0; i < 4; i++)
	{
		GridShape s;
		s.aabb.SetCenterExtents(
			vec3(((i % 2) ? 1 : -1) * 50, ((i / 2) ? 1 : -1) * 50, 0),
			vec3(options.GridSize / 2.0f, 5));
		s.ID = i;

		quads[i] = s;
	}
}
uint32_t GridHeuristic::GetBoundsCount() const {}
void GridHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const {}
void GridHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<GridShape>>& out_deltas) const
{
}
IHeuristic::Type GridHeuristic::GetType() const
{
	return IHeuristic::Type::eGridCell;
}
void GridHeuristic::SerializeBounds(
	std::unordered_map<IBounds::BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const GridShape& quad : quads)
	{
		auto [it, inserted] = bws.emplace(quad.ID, ByteWriter{});
		quad.Serialize(it->second);
	}
}
void GridHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(quads.size());
	for (const auto& quad : quads)
	{
		quad.Serialize(bw);
	}
};
void GridHeuristic::Deserialize(ByteReader& br)
{
	const size_t quadcount = br.u64();
	quads.resize(quadcount);
	for (int i = 0; i < quads.size(); i++)
	{
		quads[i].Deserialize(br);
	}
};
std::unique_ptr<IBounds> GridHeuristic::QueryPosition(vec3 p)
{
	logger.DebugFormatted("Querying position");

	for (const auto& shape : quads)
	{
		if (shape.aabb.contains(p))
		{
			logger.DebugFormatted("Found the one");

			std::unique_ptr<GridShape> s = std::make_unique<GridShape>(shape);
			return s;
		}
	}
	logger.DebugFormatted("Found the one");

	return nullptr;
}

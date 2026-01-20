#include "GridHeuristic.hpp"
#include "Serialize/ByteWriter.hpp"
GridHeuristic::GridHeuristic(const Options& _options) : options(_options) {}
void GridHeuristic::Compute(const AtlasEntitySpan<const AtlasEntityMinimal>& span) {}
uint32_t GridHeuristic::GetBoundsCount() const {}
void GridHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const {}
void GridHeuristic::GetBoundDeltas(std::vector<TBoundDelta<GridShape>>& out_deltas) const {}
IHeuristic::Type GridHeuristic::GetType() const
{
	return IHeuristic::Type::eGridCell;
}
void GridHeuristic::SerializeBounds(std::unordered_map<IBounds::BoundsID, ByteWriter>& bws)
{
	for (int i = 0; i < 4; i++)
	{
		GridShape s;
		s.SetCenterExtents(vec3(((i % 2) ? 1 : -1) * 50, ((i / 2) ? 1 : -1) * 50, 0),
							 vec3(options.GridSize/2.0f, 0));
		s.ID = i;
		ByteWriter& bw = bws[s.ID];
		s.Serialize(bw); 
	}
}

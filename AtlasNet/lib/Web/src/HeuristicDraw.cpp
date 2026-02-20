#include "HeuristicDraw.hpp"

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Network/NetworkIdentity.hpp"
#include <cstdio>
#include <iostream>
#include <unordered_map>
void HeuristicDraw::DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes)
{
	std::cout << "Fetching Shapes" << std::endl;
	shapes.clear();
	std::vector<GridShape> out_grid_shape;
	HeuristicManifest::Get().GetAllPendingBounds<GridShape>(out_grid_shape);
	for (const auto& grid:out_grid_shape)
	{
		IBoundsDrawShape rect;
		rect.pos_x = grid.aabb.center().x;
		rect.pos_y = grid.aabb.center().y;
		rect.type = IBoundsDrawShape::Type::eRectangle;
		rect.size_x = grid.aabb.halfExtents().x*2;
		rect.size_y = grid.aabb.halfExtents().y*2;
		rect.id = grid.ID;
		rect.owner_id = "";
		rect.color = "rgba(255, 149, 100, 1)";
		shapes.emplace_back(rect);
	}
	std::unordered_map<NetworkIdentity, GridShape> claimed_bounds;
	HeuristicManifest::Get().GetAllClaimedBounds(claimed_bounds);
	for (const auto& [claim_key, grid] : claimed_bounds)
	{
		IBoundsDrawShape rect;
		rect.pos_x = grid.aabb.center().x;
		rect.pos_y = grid.aabb.center().y;
		rect.type = IBoundsDrawShape::Type::eRectangle;
		rect.size_x = grid.aabb.halfExtents().x*2;
		rect.size_y = grid.aabb.halfExtents().y*2;
		rect.id = grid.ID;
		rect.owner_id = claim_key.ToString();
		rect.color = "rgba(100, 255, 149, 1)";
		shapes.emplace_back(rect);
	}

	std::printf("returning %zu shapes", shapes.size());
}

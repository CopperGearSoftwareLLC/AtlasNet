#include "HeuristicDraw.hpp"

#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include <cstdio>
#include <iostream>
void HeuristicDraw::DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes)
{
	std::cout << "Fetching Shapes" << std::endl;
	shapes.clear();
	std::vector<GridShape> out_grid_shape;
	HeuristicManifest::Get().GetAllPendingBounds<GridShape>(out_grid_shape);
	for (const auto& grid:out_grid_shape)
	{
		IBoundsDrawShape rect;
		rect.pos_x = grid.center().x;
		rect.pos_y = grid.center().y;
		rect.type = IBoundsDrawShape::Type::eRectangle;
		rect.size_x = grid.halfExtents().x*2;
		rect.size_y = grid.halfExtents().y*2;
		rect.id = grid.ID;
		shapes.emplace_back(rect);
	}
	std::printf("retrieved %i shapes from Heuristic manifest",out_grid_shape.size());
	IBoundsDrawShape rect;
	rect.pos_x = 50;
	rect.pos_y = 50;
	rect.id = 0;
	rect.size_x = 100;
	rect.size_y = 100;
	rect.type = IBoundsDrawShape::Type::eRectangle;
	shapes.emplace_back(rect);

	IBoundsDrawShape circle;
	circle.pos_x = -50;
	circle.pos_y = 50;
	circle.id = 0;
	circle.radius = 20;
	circle.type = IBoundsDrawShape::Type::eCircle;
	shapes.emplace_back(circle);

	IBoundsDrawShape polygon;
	polygon.pos_x = -50;
	polygon.pos_y = -0;
	polygon.id = 0;
	polygon.verticies = {{60,-60},{-40,-55},{80,-140}};
	polygon.type = IBoundsDrawShape::Type::ePolygon;
	shapes.emplace_back(polygon);
	std::printf("returning %i shapes",shapes.size());
}

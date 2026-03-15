#include "HeuristicDraw.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiBounds.hpp"
#include "Network/NetworkIdentity.hpp"
void HeuristicDraw::DrawCurrentHeuristic(std::vector<IBoundsDrawShape>& shapes)
{
	shapes.clear();
	HeuristicManifest::Get().PullHeuristic(
		[&](const IHeuristic& h)
		{
			switch (h.GetType())
			{
				case IHeuristic::Type::eNone:
				case IHeuristic::Type::eInvalid:
				case IHeuristic::Type::eOctree:
					ASSERT(false, "INVALID HEURISTIC");
					break;

				case IHeuristic::Type::eGridCell:
				case IHeuristic::Type::eQuadtree:
				{
					h.ForEachBound(
						[&](const IBounds& b)
						{
							const auto* gsp = dynamic_cast<const GridShape*>(&b);
							ASSERT(gsp, "INVALID BOUND SHAPE");
							const GridShape& gs = *gsp;
							IBoundsDrawShape rect;
							rect.pos_x = gs.aabb.center().x;
							rect.pos_y = gs.aabb.center().y;
							rect.type = IBoundsDrawShape::Type::eRectangle;
							rect.size_x = gs.aabb.halfExtents().x * 2;
							rect.size_y = gs.aabb.halfExtents().y * 2;
							rect.id = gs.ID;
							shapes.emplace_back(rect);
						});
				}
				break;

				case IHeuristic::Type::eVoronoi:
				case IHeuristic::Type::eHotspotVoronoi:
				case IHeuristic::Type::eLlmVoronoi:
				{
					h.ForEachBound(
						[&](const IBounds& b)
						{
							const auto* vsp = dynamic_cast<const VoronoiBounds*>(&b);
							ASSERT(vsp, "INVALID BOUND SHAPE");
							const VoronoiBounds& polyBound = *vsp;
							IBoundsDrawShape poly;
							poly.id = polyBound.ID;
							ASSERT(poly.id >= 0, "INVALID VORONOI BOUND ID");

							poly.type = IBoundsDrawShape::Type::ePolygon;
							const vec3 c = polyBound.GetCenter();
							poly.pos_x = c.x;
							poly.pos_y = c.y;
							poly.verticies.clear();
							poly.verticies.reserve(polyBound.vertices.size());
							for (const auto& v : polyBound.vertices)
							{
								// Cartograph expects polygon vertices as local offsets from
								// position.
								poly.verticies.emplace_back(v.x - c.x, v.y - c.y);
							}
							poly.half_planes.clear();
							poly.half_planes.reserve(polyBound.halfPlanes.size() * 3);
							for (const auto& plane : polyBound.halfPlanes)
							{
								poly.half_planes.push_back(plane.normal.x);
								poly.half_planes.push_back(plane.normal.y);
								poly.half_planes.push_back(plane.c);
							}

							shapes.emplace_back(std::move(poly));
						});
				}
				break;
			}
		});
	HeuristicManifest::Get().QueryOwnershipState(
		[&](const HeuristicManifest::OwnershipStateWrapper& o)
		{
			for (auto& shape : shapes)
			{
				if (const auto shard = o.GetBoundOwner(shape.id); shard.has_value())
				{
					shape.color = "rgba(100, 255, 149, 1)";
					shape.owner_id = shard->ToString();
				}
				else
				{
					shape.color = "rgba(255, 149, 100, 1)";
				}
			}
		});

	std::printf("returning %zu shapes", shapes.size());
}

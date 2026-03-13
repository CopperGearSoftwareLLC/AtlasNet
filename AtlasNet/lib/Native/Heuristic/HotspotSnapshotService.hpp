#pragma once

#include <cstddef>

#include "Heuristic/Voronoi/VoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"

class HotspotSnapshotService
{
   public:
	static void StoreSnapshot(
		const HotspotVoronoiHeuristic& heuristic, size_t entityCount);
	static void StoreSnapshot(
		const VoronoiHeuristic& heuristic, size_t entityCount, uint32_t availableServerCount);
};

#include "HeuristicService.hpp"

#include <thread>

#include "Entity/Transform.hpp"
#include "Events/GlobalEvents.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/Events/HeuristicUpdateEvent.hpp"
#include "Heuristic/HotspotSnapshotService.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Network/NetworkEnums.hpp"
#include "Shard/ShardService.hpp"
#include "Snapshot/SnapshotService.hpp"

namespace
{
uint32_t ResolveAvailableServerCount()
{
	uint32_t shardCount = 0;
	for (const auto& [id, _entry] : ServerRegistry::Get().GetServers())
	{
		if (id.Type == NetworkIdentityType::eShard)
		{
			++shardCount;
		}
	}

	return shardCount;
}

uint32_t ResolveHotspotCountFromServerCount(const uint32_t serverCount)
{
	const uint32_t effectiveServerCount = std::max<uint32_t>(1, serverCount);
	return ((effectiveServerCount * 3U) + 1U) / 2U;
}
}  // namespace

void HeuristicService::HeuristicThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_HEURISTIC_RECOMPUTE_INTERNAL_MS);
	while (!st.stop_requested())
	{
		// Align roughly to system clock
		logger.DebugFormatted("Computing Heuristic");
		ComputeHeuristic();
		std::this_thread::sleep_for(interval);
	}
}
void HeuristicService::ComputeHeuristic()
{
	std::vector<Transform> transforms;
	SnapshotService::Get().FetchAllTransforms(transforms);
	const uint32_t resolvedServerCount = ResolveAvailableServerCount();

	logger.DebugFormatted("Fetched {} entities", transforms.size());
	if (auto* hotspotVoronoi =
			dynamic_cast<HotspotVoronoiHeuristic*>(activeHeuristic.get()))
	{
		const uint32_t effectiveServerCount =
			resolvedServerCount > 0 ? resolvedServerCount
									: hotspotVoronoi->options.DefaultServerCount;
		hotspotVoronoi->SetAvailableServerCount(effectiveServerCount);
		hotspotVoronoi->SetHotspotCount(
			ResolveHotspotCountFromServerCount(effectiveServerCount));
	}
	activeHeuristic->Compute(std::span(transforms));
	HeuristicManifest::Get().PushHeuristic(*activeHeuristic);
	ShardService::Get().ScaleShardService(activeHeuristic->GetBoundsCount());
	if (const auto* hotspotVoronoi =
			dynamic_cast<const HotspotVoronoiHeuristic*>(activeHeuristic.get()))
	{
		HotspotSnapshotService::StoreSnapshot(*hotspotVoronoi, transforms.size());
	}
	else if (const auto* legacyVoronoi =
				 dynamic_cast<const VoronoiHeuristic*>(activeHeuristic.get()))
	{
		const uint32_t effectiveServerCount =
			resolvedServerCount > 0 ? resolvedServerCount
									: std::max<uint32_t>(1, legacyVoronoi->GetBoundsCount());
		HotspotSnapshotService::StoreSnapshot(
			*legacyVoronoi, transforms.size(), effectiveServerCount);
	}

	HeuristicUpdateEvent e;

	GlobalEvents::Get().Dispatch(e);
}

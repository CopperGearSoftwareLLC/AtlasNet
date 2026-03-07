#include "HeuristicService.hpp"

#include <thread>

#include "Entity/Transform.hpp"
#include "Events/GlobalEvents.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/Events/HeuristicUpdateEvent.hpp"
#include "Heuristic/IBounds.hpp"
#include "Shard/ShardService.hpp"
#include "Snapshot/SnapshotService.hpp"
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

	logger.DebugFormatted("Fetched {} entities", transforms.size());
	activeHeuristic->Compute(std::span(transforms));
	HeuristicManifest::Get().PushHeuristic(*activeHeuristic);
	ShardService::Get().ScaleShardService(activeHeuristic->GetBoundsCount());

	HeuristicUpdateEvent e;

	GlobalEvents::Get().Dispatch(e);
}

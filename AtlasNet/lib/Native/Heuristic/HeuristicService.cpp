#include "HeuristicService.hpp"

#include <thread>

#include "Entity/Transform.hpp"
#include "Events/GlobalEvents.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/Events/HeuristicUpdateEvent.hpp"
#include "Heuristic/HotspotSnapshotService.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Heuristic/Quadtree/QuadtreeHeuristic.hpp"
#include "Heuristic/Voronoi/HotspotVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/LlmVoronoiHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
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

std::unique_ptr<IHeuristic> HeuristicService::CreateHeuristic(
	const IHeuristic::Type type) const
{
	switch (type)
	{
		case IHeuristic::Type::eGridCell:
			return std::make_unique<GridHeuristic>();
		case IHeuristic::Type::eQuadtree:
			return std::make_unique<QuadtreeHeuristic>();
		case IHeuristic::Type::eVoronoi:
			return std::make_unique<VoronoiHeuristic>();
		case IHeuristic::Type::eHotspotVoronoi:
			return std::make_unique<HotspotVoronoiHeuristic>();
		case IHeuristic::Type::eLlmVoronoi:
			return std::make_unique<LlmVoronoiHeuristic>();
		case IHeuristic::Type::eNone:
		case IHeuristic::Type::eInvalid:
		case IHeuristic::Type::eOctree:
		default:
			return nullptr;
	}
}

void HeuristicService::SyncDesiredHeuristic()
{
	if (!activeHeuristic)
	{
		activeHeuristic = CreateHeuristic(IHeuristic::Type::eHotspotVoronoi);
	}

	const std::optional<std::string> desiredTypeRaw =
		InternalDB::Get()->Get(desiredHeuristicTypeKey);
	if (!desiredTypeRaw.has_value() || desiredTypeRaw->empty())
	{
		InternalDB::Get()->Set(
			desiredHeuristicTypeKey,
			IHeuristic::TypeToString(activeHeuristic->GetType()));
		return;
	}

	IHeuristic::Type desiredType = IHeuristic::Type::eInvalid;
	if (!IHeuristic::TypeFromString(*desiredTypeRaw, desiredType))
	{
		logger.WarningFormatted(
			"Ignoring unsupported desired heuristic type '{}'.", *desiredTypeRaw);
		return;
	}

	if (desiredType == activeHeuristic->GetType())
	{
		return;
	}

	std::unique_ptr<IHeuristic> nextHeuristic = CreateHeuristic(desiredType);
	if (!nextHeuristic)
	{
		logger.WarningFormatted(
			"Desired heuristic type '{}' is not constructible at runtime.",
			IHeuristic::TypeToString(desiredType));
		return;
	}

	logger.DebugFormatted(
		"Switching active heuristic from {} to {}",
		IHeuristic::TypeToString(activeHeuristic->GetType()),
		IHeuristic::TypeToString(desiredType));
	activeHeuristic = std::move(nextHeuristic);
}

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
	SyncDesiredHeuristic();
	std::vector<AtlasTransform> transforms;
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
	else if (auto* llmVoronoi =
				 dynamic_cast<LlmVoronoiHeuristic*>(activeHeuristic.get()))
	{
		const uint32_t effectiveServerCount =
			resolvedServerCount > 0 ? resolvedServerCount
									: llmVoronoi->options.DefaultServerCount;
		llmVoronoi->SetAvailableServerCount(effectiveServerCount);
		llmVoronoi->SetHotspotCount(
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
	else if (const auto* llmVoronoi =
				 dynamic_cast<const LlmVoronoiHeuristic*>(activeHeuristic.get()))
	{
		HotspotSnapshotService::StoreSnapshot(*llmVoronoi, transforms.size());
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

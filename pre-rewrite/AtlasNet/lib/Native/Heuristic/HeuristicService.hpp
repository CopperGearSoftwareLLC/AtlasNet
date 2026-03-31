#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>
#include <thread>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/Voronoi/VoronoiHeuristic.hpp"
class HeuristicService : public Singleton<HeuristicService>
{
	std::jthread heuristicComputeThread;
	Log logger = Log("HeuristicService");
	std::unique_ptr<IHeuristic> activeHeuristic;
	const std::string desiredHeuristicTypeKey =
		"Heuristic:DesiredType";
	const std::string recomputeModeKey = "Heuristic:RecomputeMode";
	const std::string recomputeIntervalMsKey = "Heuristic:RecomputeIntervalMs";
	const std::string recomputeRequestKey = "Heuristic:RecomputeRequest";
   private:
	enum class RecomputeMode
	{
		eInterval,
		eManual,
		eLoad,
	};

	void HeuristicThreadLoop(std::stop_token st);

	void ComputeHeuristic();
	void SyncDesiredHeuristic();
	[[nodiscard]] RecomputeMode ResolveRecomputeMode() const;
	[[nodiscard]] std::chrono::milliseconds ResolveRecomputeInterval() const;
	[[nodiscard]] bool ConsumeManualRecomputeRequest() const;
	[[nodiscard]] static std::string_view RecomputeModeToString(RecomputeMode mode);
	[[nodiscard]] std::unique_ptr<IHeuristic> CreateHeuristic(
		IHeuristic::Type type) const;

   public:
	HeuristicService()
	{
		activeHeuristic = CreateHeuristic(IHeuristic::Type::eHotspotVoronoi);
		heuristicComputeThread =
			std::jthread([this](std::stop_token st) { HeuristicThreadLoop(st); });
	}
};

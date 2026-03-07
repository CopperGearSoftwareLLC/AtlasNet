#pragma once

#include <memory>
#include <stop_token>
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
   private:
	void HeuristicThreadLoop(std::stop_token st);

	void ComputeHeuristic();

   public:
	HeuristicService()
	{
		activeHeuristic = std::make_unique<VoronoiHeuristic>();
		heuristicComputeThread =
			std::jthread([this](std::stop_token st) { HeuristicThreadLoop(st); });
	}
};
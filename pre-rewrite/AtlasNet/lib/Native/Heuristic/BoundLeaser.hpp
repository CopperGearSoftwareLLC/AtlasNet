#pragma once

#include <chrono>
#include <stop_token>
#include <thread>
#include <type_traits>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Network/NetworkIdentity.hpp"
class BoundLeaser : public Singleton<BoundLeaser>
{
	Log logger = Log("BoundLeaser");
	std::optional<BoundsID> ClaimedBoundID;

	std::jthread LoopThread;

   private:
	void LoopEntry(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			if (!ClaimedBoundID.has_value())
			{
				ClaimBound();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void ClaimBound();

   public:
	void Init()
	{
		logger.Debug("Init");
		LoopThread = std::jthread([this](std::stop_token st) { LoopEntry(st); });
	}
	[[nodiscard]] constexpr bool HasBound() const { return ClaimedBoundID.has_value(); }
	template <typename FN>
		requires std::is_invocable_v<FN, const IBounds&>
	auto GetBound(FN&& f)
	{
		HeuristicManifest::Get().PullHeuristic(
			[&](const IHeuristic& h)
			{
				if (ClaimedBoundID.has_value())
				{
					if (h.GetBoundsCount() >= ClaimedBoundID.value())
					{
						return f(h.GetBound(ClaimedBoundID.value()));
					}
					
				}
			});
	}
	[[nodiscard]] constexpr BoundsID GetBoundID() const { return ClaimedBoundID.value(); }
};

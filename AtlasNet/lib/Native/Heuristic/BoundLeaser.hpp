#pragma once

#include <chrono>
#include <mutex>
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
	mutable std::mutex ClaimedBoundMutex;

	std::jthread LoopThread;

   private:
	void LoopEntry(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			std::optional<BoundsID> claimedBoundID;
			{
				std::lock_guard lock(ClaimedBoundMutex);
				claimedBoundID = ClaimedBoundID;
			}

			if (!claimedBoundID.has_value())
			{
				ClaimBound();
			}
			else
			{
				ValidateClaimedBound(claimedBoundID.value());
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void ClaimBound();
	void ValidateClaimedBound(BoundsID claimedBoundID);
	void ClearInvalidClaimedBound(BoundsID claimedBoundID);

   public:
	void Init()
	{
		logger.Debug("Init");
		LoopThread = std::jthread([this](std::stop_token st) { LoopEntry(st); });
	}
	[[nodiscard]] bool HasBound() const
	{
		std::lock_guard lock(ClaimedBoundMutex);
		return ClaimedBoundID.has_value();
	}
	void ClearClaimedBound()
	{
		std::lock_guard lock(ClaimedBoundMutex);
		ClaimedBoundID.reset();
	}
	template <typename FN>
		requires std::is_invocable_v<FN, const IBounds&>
	void GetBound(FN&& f)
	{
		std::optional<BoundsID> claimedBoundID;
		{
			std::lock_guard lock(ClaimedBoundMutex);
			claimedBoundID = ClaimedBoundID;
		}
		if (!claimedBoundID.has_value())
		{
			return;
		}
		try
		{
			HeuristicManifest::Get().PullHeuristic(
				[&](const IHeuristic& h)
				{
					f(h.GetBound(claimedBoundID.value()));
				});
		}
		catch (const std::exception& ex)
		{
			logger.WarningFormatted(
				"Claimed bound {} is no longer valid in the active heuristic. Clearing local claim. {}",
				claimedBoundID.value(), ex.what());
			ClearInvalidClaimedBound(claimedBoundID.value());
		}
		catch (...)
		{
			logger.WarningFormatted(
				"Claimed bound {} is no longer valid in the active heuristic. Clearing local claim.",
				claimedBoundID.value());
			ClearInvalidClaimedBound(claimedBoundID.value());
		}
	}
	[[nodiscard]] BoundsID GetBoundID() const
	{
		std::lock_guard lock(ClaimedBoundMutex);
		return ClaimedBoundID.value();
	}
};

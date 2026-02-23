#pragma once

#include <chrono>
#include <stop_token>
#include <thread>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IBounds.hpp"
#include "Network/NetworkIdentity.hpp"
class BoundLeaser : public Singleton<BoundLeaser>
{
	Log logger = Log("BoundLeaser");
	std::unique_ptr<IBounds> ClaimedBound;
	IBounds::BoundsID ClaimedBoundID;

	std::jthread LoopThread;

   private:
	void LoopEntry(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			if (!ClaimedBound)
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
	[[nodiscard]] constexpr bool HasBound() const { return ClaimedBound != nullptr; }
	[[nodiscard]] constexpr const IBounds& GetBound() const { return *ClaimedBound; }
	
};

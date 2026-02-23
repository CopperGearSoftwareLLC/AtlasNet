#pragma once

#include <cstdint>
#include <stop_token>
#include <thread>
#include "Heuristic/IHeuristic.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Global/pch.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Debug/Log.hpp"
class WatchDog : public Singleton<WatchDog>
{
    public:
    void ClearAllDatabaseState();

    private:
        std::shared_ptr<Log> logger = std::make_shared<Log>("WatchDog");

    std::atomic_bool ShouldShutdown = false;
    uint32_t ShardCount = 0;
    NetworkIdentity ID = NetworkIdentity::MakeIDWatchDog();

    IHeuristic::Type ActiveHeuristic = IHeuristic::Type::eNone;
    std::shared_ptr<IHeuristic> Heuristic;
    std::jthread HeuristicThread;
public:
    WatchDog();
    ~WatchDog();

    void Shutdown()
    {
        ShouldShutdown = true;
    }
	void Run();

   private:
	void ComputeHeuristic();
	void SwitchHeuristic(IHeuristic::Type newHeuristic);
	void HeuristicThreadEntry(std::stop_token);
	void AuditActiveHandoffTransfers();
	void Init();
   
    void Cleanup();
    /**
     * @brief Sets the new number of Shard
     */
    void SetShardCount(uint32_t NewCount);
    /**
     * @brief Set of active partition IDs.
     * @brief Handles termination signals to ensure cleanup of partitions before exiting. (doesnt work rn)
     */
    static void handleSignal(int32_t signum);
};

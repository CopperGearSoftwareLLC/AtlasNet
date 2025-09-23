#pragma once
#include "pch.hpp"
#include "Singleton.hpp"

class God : public Singleton<God> {
   public:
    God();
    ~God();

    /**
     * @brief Spawns a new partition by invoking an external script.
     */
    bool spawnPartition(int32 id, int32 port);

    /**
     * @brief Removes a partition by its ID.
     */
    bool removePartition(int32 id);

    /**
     * @brief Cleans up all active partitions.
     */
    bool cleanupPartitions();

    /**
     * @brief Retrieves a set of all active partition IDs.
     */
    std::set<int32> getPartitionIDs();

    /**
     * @brief Get the Partition object (returns null for now)
     */
    void* getPartition(int32 id);

    private:
    /**
     * @brief Set of active partition IDs.
     */
    std::set<int32> partitionIds;

    /**
     * @brief Handles termination signals to ensure cleanup of partitions before exiting. (doesnt work rn)
     */
    static void handleSignal(int32 signum);
};
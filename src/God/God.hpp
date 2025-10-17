#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Interlink/Interlink.hpp"
#include "Heuristic/Heuristic.hpp"
#include "Database/RedisCacheDatabase.hpp"
class God : public Singleton<God>
{
public:
    struct ActiveContainer
    {
        Json LatestInformJson;
        DockerContainerID ID;
    };

private:
    CURL *curl;
    std::shared_ptr<Log> logger = std::make_shared<Log>("God");
    Heuristic heuristic;
    struct IndexByID
    {
    };
    boost::multi_index_container<
        ActiveContainer,
        boost::multi_index::indexed_by<
            // Unique By ID
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<IndexByID>,
                boost::multi_index::member<ActiveContainer, DockerContainerID,
                                           &ActiveContainer::ID>>

            >>
        ActiveContainers;
    bool ShouldShutdown = false;

public:
    God();
    ~God();

    void Shutdown()
    {
        ShouldShutdown = true;
    }

    void Init();

    /**
     * @brief Computes partition shapes using heuristic algorithms and stores them in the database.
     *
     * This function calls the Heuristic::computePartition() method to generate optimal
     * partition boundaries, then serializes and stores the resulting shapes in the
     * cache database for persistence and retrieval by other system components.
     *
     * @return bool True if the operation completed successfully, false otherwise.
     */
    bool computeAndStorePartitions();


    
    private:
    void HeuristicUpdate();
    /**
     * @brief Retrieves a set of all active partition IDs.
     */
    const decltype(ActiveContainers) &GetContainers();

    /**
     * @brief Get the Partition object (returns null for now)
     */
    const ActiveContainer &GetContainer(const DockerContainerID &id);

    /**
     * @brief Notifies all partitions to fetch their shape data
     */
    void notifyPartitionsToFetchShapes();

    /**
     * @brief Spawns a new partition by invoking an external script.
     */
    std::optional<ActiveContainer> spawnPartition();

    /**
     * @brief Removes a partition by its ID.
     */
    bool removePartition(const DockerContainerID &id, uint32 TimeOutSeconds = 10);

    /**
     * @brief Cleans up all active partitions.
     */
    bool cleanupContainers();
    /**
     * @brief Set of active partition IDs.
     */
    std::set<int32> partitionIds;

    /**
     * @brief Cache database pointer for storing partition metadata.
     *
     */
    std::unique_ptr<IDatabase> cache;

    /**
     * @brief Handles termination signals to ensure cleanup of partitions before exiting. (doesnt work rn)
     */
    static void handleSignal(int32 signum);
};
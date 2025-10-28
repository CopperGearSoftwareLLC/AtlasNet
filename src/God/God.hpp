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
    private:
        CURL *curl;
        std::shared_ptr<Log> logger = std::make_shared<Log>("God");
        Heuristic heuristic;
        uint32 PartitionCount = 0;
    std::atomic_bool ShouldShutdown = false;



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
    std::vector<std::string>GetPartitionIDs();
    /**
     * @brief Notifies all partitions to fetch their shape data
     */
    void notifyPartitionsToFetchShapes();

    void Cleanup();
    /**
     * @brief Sets the new number of partitions
     */
    void SetPartitionCount(uint32 NewCount);
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
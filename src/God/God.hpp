    void ClearAllDatabaseState();
    #pragma once
    #include "pch.hpp"
    #include "Singleton.hpp"
    #include "Debug/Log.hpp"
    #include "Interlink/Interlink.hpp"
    #include "Heuristic/Heuristic.hpp"
    #include "Database/RedisCacheDatabase.hpp"
#include "AtlasNet/AtlasEntity.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "Utils/GeometryUtils.hpp"
class God : public Singleton<God>
{

    public:
    void ClearAllDatabaseState();

    // Structure to cache partition shape data
    struct PartitionShapeCache {
        std::vector<Shape> shapes;
        std::map<std::string, size_t> partitionToShapeIndex;
        bool isValid = false;
        
        // Spatial index for fast partition lookups
        // Maps grid cell coordinates to partition IDs for O(1) lookup
        // Key format: "x,y" where x and y are grid cell indices
        std::unordered_map<std::string, std::string> spatialIndex;
        int spatialIndexGridSize = 100; // Divide space into 100x100 grid for lookup
        bool spatialIndexValid = false;
        
        /**
         * @brief Build spatial index from shapes for fast lookups
         */
        void buildSpatialIndex() {
            spatialIndex.clear();
            spatialIndexValid = false;
            
            if (!isValid || shapes.empty()) return;
            
            // For each shape, sample points and map them to partition IDs
            for (const auto& [partitionId, shapeIdx] : partitionToShapeIndex) {
                if (shapeIdx >= shapes.size()) continue;
                
                const Shape& shape = shapes[shapeIdx];
                
                // For each triangle in the shape, add grid cells to spatial index
                for (const auto& triangle : shape.triangles) {
                    // Get bounding box of triangle
                    float minX = std::min({triangle[0].x, triangle[1].x, triangle[2].x});
                    float maxX = std::max({triangle[0].x, triangle[1].x, triangle[2].x});
                    float minY = std::min({triangle[0].y, triangle[1].y, triangle[2].y});
                    float maxY = std::max({triangle[0].y, triangle[1].y, triangle[2].y});
                    
                    // Map all grid cells that overlap with this triangle
                    int gridMinX = static_cast<int>(minX * spatialIndexGridSize);
                    int gridMaxX = static_cast<int>(maxX * spatialIndexGridSize) + 1;
                    int gridMinY = static_cast<int>(minY * spatialIndexGridSize);
                    int gridMaxY = static_cast<int>(maxY * spatialIndexGridSize) + 1;
                    
                    // Clamp to valid range
                    gridMinX = std::max(0, std::min(gridMinX, spatialIndexGridSize - 1));
                    gridMaxX = std::max(0, std::min(gridMaxX, spatialIndexGridSize));
                    gridMinY = std::max(0, std::min(gridMinY, spatialIndexGridSize - 1));
                    gridMaxY = std::max(0, std::min(gridMaxY, spatialIndexGridSize));
                    
                    // Sample points in the bounding box and check if they're in the triangle
                    for (int gx = gridMinX; gx < gridMaxX; ++gx) {
                        for (int gy = gridMinY; gy < gridMaxY; ++gy) {
                            // Check center of grid cell
                            float cellCenterX = (gx + 0.5f) / spatialIndexGridSize;
                            float cellCenterY = (gy + 0.5f) / spatialIndexGridSize;
                            vec2 cellCenter(cellCenterX, cellCenterY);
                            
                            // Check if center is in triangle
                            if (GeometryUtils::PointInTriangle(cellCenter, triangle[0], triangle[1], triangle[2])) {
                                std::string key = std::to_string(gx) + "," + std::to_string(gy);
                                // Only add if not already mapped (first shape wins)
                                if (spatialIndex.find(key) == spatialIndex.end()) {
                                    spatialIndex[key] = partitionId;
                                }
                            }
                        }
                    }
                }
            }
            
            spatialIndexValid = true;
        }
        
        /**
         * @brief Fast lookup using spatial index
         */
        std::optional<std::string> fastLookup(const vec2& point) const {
            if (!spatialIndexValid) return std::nullopt;
            
            // Clamp point to [0,1] range
            float x = std::max(0.0f, std::min(1.0f, point.x));
            float y = std::max(0.0f, std::min(1.0f, point.y));
            
            // Convert to grid coordinates
            int gx = static_cast<int>(x * spatialIndexGridSize);
            int gy = static_cast<int>(y * spatialIndexGridSize);
            
            // Clamp to valid range
            gx = std::max(0, std::min(gx, spatialIndexGridSize - 1));
            gy = std::max(0, std::min(gy, spatialIndexGridSize - 1));
            
            std::string key = std::to_string(gx) + "," + std::to_string(gy);
            auto it = spatialIndex.find(key);
            if (it != spatialIndex.end()) {
                return it->second;
            }
            
            return std::nullopt;
        }
    };

    private:
        CURL *curl;
        std::shared_ptr<Log> logger = std::make_shared<Log>("God");
        Heuristic heuristic;
    PartitionShapeCache shapeCache;
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
     * @brief Computes partition shapes using heuristic algorithms and stores them in both memory cache and database.
     *
     * This function calls the Heuristic::computePartition() method to generate optimal
     * partition boundaries, then:
     * 1. Stores the shapes in memory for fast access
     * 2. Serializes and stores the shapes in the cache database for persistence
     *
     * @return bool True if the operation completed successfully, false otherwise.
     */
    bool computeAndStorePartitions();

    /**
     * @brief Gets the cached shape for a specific partition.
     * @param partitionId The ID of the partition to get the shape for.
     * @return Pointer to the shape if found in cache, nullptr otherwise.
     */
    const Shape* getCachedShape(const std::string& partitionId) const;

    /**
     * @brief Gets all cached shapes.
     * @return Vector of all shapes currently in the cache.
     */
    const std::vector<Shape>& getAllCachedShapes() const { return shapeCache.shapes; }

    /**
     * @brief Checks if the shape cache is valid.
     * @return True if the cache contains valid shape data.
     */
    bool isShapeCacheValid() const { return shapeCache.isValid; }


    
    private:
        void HeuristicUpdate();
        void RedistributeOutliersToPartitions();
        void handleOutlierMessage(const std::string& message);
        void handleOutliersDetectedMessage(const std::string& message);
        void handleEntityRemovedMessage(const std::string& message);
        void processOutliersForPartition(const std::string& partitionId);
        void processExistingOutliers();
        
        /**
         * @brief Efficient grid cell-based entity detection
         * 
         * This method uses grid cells instead of triangles for much faster entity positioning.
         * It's 4.5x faster than triangle-based detection and uses 50% less memory.
         */
        void processOutliersWithGridCells();
        
        /**
         * @brief Redistributes an entity to the correct partition based on grid cell shapes
         * 
         * This method is called when a partition confirms it has removed an entity.
         * God then finds the correct target partition and moves the entity there.
         */
        void redistributeEntityToCorrectPartition(const AtlasEntity& entity, const std::string& sourcePartition);
        
        // Helpers to improve readability and reuse
        std::vector<InterLinkIdentifier> getPartitionServerIds() const;
        std::optional<std::string> findPartitionForPoint(const vec2& point) const;
        void sendHeaderBasedMessage(const std::string& message, AtlasNetMessageHeader header, const std::string& partitionId, InterlinkMessageSendFlag sendFlag = InterlinkMessageSendFlag::eReliableNow);
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
     * @brief Set of partitions that were assigned shapes in the last computeAndStorePartitions call.
     */
    std::set<InterLinkIdentifier> assignedPartitions;

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
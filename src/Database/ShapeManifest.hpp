#pragma once
#include "pch.hpp"
#include "IDatabase.hpp"
#include "Heuristic/Shape.hpp"

/**
 * @brief Database manifest for partition shapes
 */
class ShapeManifest {
public:
    static inline const std::string HASH_KEY = "PartitionShapeManifest";
    static inline const std::string METADATA_KEY = "shape_manifest_metadata";

    /**
     * @brief Store a shape for a partition
     */
    static bool Store(IDatabase* db, const std::string& partitionId, int shapeId, const Shape& shape) {
        if (!db) return false;

        std::string shapeData;
        shapeData += "shape_id:" + std::to_string(shapeId) + ";";
        shapeData += "partition_id:" + partitionId + ";";
        shapeData += "triangles:";

        for (const auto& triangle : shape.triangles) {
            shapeData += "triangle:";
            for (const auto& vertex : triangle) {
                shapeData += "v(" + std::to_string(vertex.x) + "," + 
                            std::to_string(vertex.y) + ");";
            }
        }

        return db->HashSet(HASH_KEY, partitionId, shapeData);
    }

    /**
     * @brief Fetch a shape for a partition
     */
    static std::optional<std::string> Fetch(IDatabase* db, const std::string& partitionId) {
        if (!db) return std::nullopt;
        
        std::string data = db->HashGet(HASH_KEY, partitionId);
        if (data.empty()) return std::nullopt;
        
        return data;
    }


    /// @brief Fetch all shapes and the partitions they belong to
    /// @param db 
    /// @return 
    static std::unordered_map<std::string,std::string> FetchAll(IDatabase* db)
    {
        if (!db) return {};
        ASSERT(db,"Invalid Database");
        return db->HashGetAll(HASH_KEY);
    }
    /**
     * @brief Store metadata about the shape assignments
     */
    static bool StoreMetadata(IDatabase* db, size_t numShapes, size_t numPartitions) {
        if (!db) return false;

        std::string metadata = "total_shapes:" + std::to_string(numShapes) +
                             ";total_partitions:" + std::to_string(numPartitions) +
                             ";timestamp:" + std::to_string(std::time(nullptr));

        return db->Set(METADATA_KEY, metadata);
    }

    /**
     * @brief Clear all shapes from the manifest
     */
    static bool Clear(IDatabase* db) {
        if (!db) return false;
        return db->HashRemoveAll(HASH_KEY);
    }
};
#pragma once
#include "InternalDB.hpp"
#include "pch.hpp"
#include "Shape.hpp"

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
    static bool Store(const std::string& partitionId, int shapeId, const Shape& shape) {

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

        return InternalDB::Get()->HSet(HASH_KEY, partitionId, shapeData);
    }

    /**
     * @brief Fetch a shape for a partition
     */
    static std::optional<std::string> Fetch( const std::string& partitionId) {
        
        std::string data = InternalDB::Get()->HGet(HASH_KEY, partitionId).value();
        if (data.empty()) return std::nullopt;
        
        return data;
    }


    /// @brief Fetch all shapes and the partitions they belong to
    /// @param db 
    /// @return 
    static std::unordered_map<std::string,std::string> FetchAll()
    {
        return InternalDB::Get()->HGetAll(HASH_KEY);
    }
    /**
     * @brief Store metadata about the shape assignments
     */
    static bool StoreMetadata( size_t numShapes, size_t numPartitions) {

        std::string metadata = "total_shapes:" + std::to_string(numShapes) +
                             ";total_partitions:" + std::to_string(numPartitions) +
                             ";timestamp:" + std::to_string(std::time(nullptr));

     InternalDB::Get()->Set(METADATA_KEY, metadata);
     return true;
    }

    /**
     * @brief Clear all shapes from the manifest
     */
    static bool Clear() {
        return InternalDB::Get()->HDelAll(HASH_KEY);
    }
};
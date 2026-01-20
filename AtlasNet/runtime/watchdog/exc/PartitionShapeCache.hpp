#pragma once
#include "pch.hpp"
#include "Shape.hpp"
#include "Misc/GeometryUtils.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <optional>

/**
 * @brief Caches partition shape data and provides fast spatial lookups
 * 
 * This cache stores partition shapes, maintains a mapping from partition IDs to shape indices,
 * and builds a spatial index for O(1) partition lookups based on point coordinates.
 */
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
     * 
     * Creates a 100x100 grid overlay on the normalized space [0,1] x [0,1] and maps
     * each grid cell to the partition ID that contains it. This enables O(1) lookups
     * for entity placement.
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
     * 
     * Converts a point to grid coordinates and performs an O(1) hash map lookup
     * to find which partition contains the point.
     * 
     * @param point The 2D point to look up (normalized coordinates [0,1] x [0,1])
     * @return The partition ID if found, std::nullopt otherwise
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


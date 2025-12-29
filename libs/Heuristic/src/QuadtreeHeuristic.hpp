#pragma once
#include "pch.hpp"
#include "Shape.hpp"
#include "AtlasEntity.hpp"

/**
 * @brief Quadtree node structure for spatial partitioning
 * 
 * Each node represents a rectangular region in 2D space [0,1] x [0,1].
 * Nodes can be subdivided into 4 quadrants for adaptive spatial partitioning.
 */
struct QuadtreeNode {
    vec2 min, max;  // Bounding box corners
    std::unique_ptr<QuadtreeNode> topLeft;
    std::unique_ptr<QuadtreeNode> topRight;
    std::unique_ptr<QuadtreeNode> bottomLeft;
    std::unique_ptr<QuadtreeNode> bottomRight;
    bool isLeaf;
    int depth;
    int maxDepth;
    int entityCount;  // Number of entities within this node's bounds
    
    /**
     * @brief Construct a quadtree node
     * @param minCorner Minimum corner of bounding box
     * @param maxCorner Maximum corner of bounding box
     * @param d Current depth in tree (default: 0)
     * @param md Maximum depth allowed (default: 4)
     */
    QuadtreeNode(vec2 minCorner, vec2 maxCorner, int d = 0, int md = 4)
        : min(minCorner), max(maxCorner), isLeaf(true), depth(d), maxDepth(md), entityCount(0) {}
    
    /**
     * @brief Check if a point is within this node's bounds
     * @param point 2D point to check
     * @return true if point is within bounds, false otherwise
     */
    bool contains(const vec2& point) const {
        return point.x >= min.x && point.x <= max.x && 
               point.y >= min.y && point.y <= max.y;
    }
    
    /**
     * @brief Count entities within this node's bounds
     * @param entities Vector of all entities to check
     */
    void countEntities(const std::vector<AtlasEntity>& entities) {
        entityCount = 0;
        for (const auto& entity : entities) {
            vec2 pos(entity.Position.x, entity.Position.z);
            if (contains(pos)) {
                entityCount++;
            }
        }
    }
    
    /**
     * @brief Subdivide this node into 4 quadrants
     * Creates four child nodes: bottomLeft, bottomRight, topLeft, topRight
     */
    void subdivide();
    
    /**
     * @brief Collect all leaf nodes and convert them to shapes
     * @param shapes Output vector to append shapes to
     * @param partitionIndex Current partition index (incremented for each leaf)
     */
    void collectLeafShapes(std::vector<Shape>& shapes, int& partitionIndex);
};

namespace QuadtreeHeuristic {
    /**
     * @brief Computes partition shapes using uniform quadtree subdivision
     * 
     * Creates a quadtree that recursively subdivides the space uniformly,
     * then converts each leaf node to a Shape for compatibility with the existing system.
     * This creates a fixed number of partitions (64 for maxDepth=3).
     * 
     * @return std::vector<Shape> Collection of shapes representing quadtree partitions
     */
    std::vector<Shape> computeQuadtreeShape();
    
    /**
     * @brief Computes partition shapes using density-based quadtree subdivision
     * 
     * Creates an adaptive quadtree that subdivides more in areas with high entity density
     * and less in sparse areas. This provides better load balancing across partitions
     * by creating more partitions where entities are clustered.
     * 
     * @param entities Vector of entities to use for density calculation
     * @param maxDepth Maximum depth of quadtree subdivision (default: 4)
     * @param minEntitiesPerNode Minimum entities required to subdivide a node (default: 4)
     * @return std::vector<Shape> Collection of shapes representing quadtree partitions
     */
    std::vector<Shape> computeQuadtreeShapeDensity(
        const std::vector<AtlasEntity>& entities,
        int maxDepth = 4,
        int minEntitiesPerNode = 4
    );
}


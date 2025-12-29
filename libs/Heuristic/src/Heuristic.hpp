#pragma once
#include <vector> 
#include "Shape.hpp"
#include "GridCell.hpp"
#include "pch.hpp"
#include "AtlasEntity.hpp"

enum HeuristicType
{
    BigSquare,
    QuadTree,
    KDTree,
    Grid
};

class Heuristic
{
public:
    Heuristic(HeuristicType type = QuadTree) : currentType(type) {}
    
    /**
     * @brief Set the heuristic type to use
     */
    void setHeuristicType(HeuristicType type) { currentType = type; }
    
    /**
     * @brief Get the current heuristic type
     */
    HeuristicType getHeuristicType() const { return currentType; }
    
    /**
     * @brief Computes partition shapes using the current heuristic type
     * @param entities Optional vector of entities for density-based algorithms
     * @return std::vector<Shape> Collection of shapes representing partitions
     */
    std::vector<Shape> computePartition(const std::vector<AtlasEntity>& entities = {});
    
    /**
     * @brief Finds which partition index a point belongs to using the current heuristic
     * @param point The point to look up (normalized coordinates [0,1] x [0,1])
     * @param shapes The computed partition shapes (from computePartition)
     * @return Optional partition index if found, nullopt otherwise
     */
    std::optional<size_t> findPartitionForPoint(const vec2& point, const std::vector<Shape>& shapes) const;
    
    /**
     * @brief Computes partition shapes using efficient grid cells instead of triangles
     * @return std::vector<GridShape> Collection of grid-based partition shapes
     */
    std::vector<GridShape> computeGridCellShapes();
    
private:
    HeuristicType currentType;
    std::vector<Shape> computeBigSquareShape();
    std::vector<Shape> computeGridShape();
};

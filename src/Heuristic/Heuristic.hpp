#pragma once
#include <vector> 
#include "Shape.hpp"
#include "GridCell.hpp"
#include "pch.hpp"

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
    Heuristic() = default;
    std::vector<Shape> computePartition();
    
    /**
     * @brief Computes partition shapes using efficient grid cells instead of triangles
     * @return std::vector<GridShape> Collection of grid-based partition shapes
     */
    std::vector<GridShape> computeGridCellShapes();
    
private:
    std::vector<Shape> computeBigSquareShape();
    std::vector<Shape> computeGridShape();
};

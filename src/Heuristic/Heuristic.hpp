#pragma once
#include <vector> 
#include "Shape.hpp"
#include "pch.hpp"
enum HeuristicType
{
    BigSquare,
    QuadTree,
    KDTree
};
class Heuristic
{
public:
    Heuristic() = default;

    std::vector<Shape> computePartition();
    std::vector<Shape> computeBigSquareShape();

};

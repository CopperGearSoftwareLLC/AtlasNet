#pragma once
#include <vector> 
#include "Shape.hpp"

class Heuristic
{
public:
    Heuristic() = default;

    // Returns a list of partition shapes (each shape is a collection of triangles).
    // "god" can call this to get the computed partition polygons.
    std::vector<Shape> computePartition();
};

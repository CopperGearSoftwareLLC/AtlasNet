#pragma once
#include <vector> 
#include "Shape.hpp"

class Heuristic
{
public:
    Heuristic() = default;

    std::vector<Shape> computePartition();
};

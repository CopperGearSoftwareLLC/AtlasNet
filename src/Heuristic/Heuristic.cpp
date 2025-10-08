#include "Heuristic.hpp"

// Example implementation: returns a single triangular shape.
// Replace this withreal heuristic logic later.
std::vector<Shape> Heuristic::computePartition()
{
    std::vector<Shape> result;

    Shape s;
    Triangle t;
    t[0] = {0.0f, 0.0f};
    t[1] = {1.0f, 0.0f};
    t[2] = {0.0f, 1.0f};

    s.triangles.push_back(t);
    result.push_back(std::move(s));

    return result;
}

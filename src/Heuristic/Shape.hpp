#pragma once
#include <vector>
#include <array>
#include <glm/vec2.hpp>


using Triangle = std::array<glm::vec2, 3>;

struct Shape
{
    // A shape is represented as a list of triangles that together form the partition polygon.
    std::vector<Triangle> triangles;
};

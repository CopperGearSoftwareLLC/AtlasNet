#pragma once
#include <vector>
#include <array>


/**
 * @brief A triangle represented as an array of three 2D vertices.
 * 
 * Each triangle is defined by three vertices using glm::vec2 for 2D coordinates.
 * The vertices are ordered counter-clockwise for proper rendering and geometric calculations.
 */
using Triangle = std::array<vec2, 3>;

/**
 * @brief Represents a geometric shape composed of triangular elements.
 * 
 * A Shape is used to represent partition polygons in the spatial partitioning system.
 * Complex polygons are decomposed into triangles for efficient rendering and geometric operations.
 * This structure is fundamental to the heuristic-based partitioning algorithm.
 */
struct Shape
{
    /**
     * @brief Collection of triangles that form the complete shape.
     * 
     * Each triangle represents a portion of the partition polygon. Together, these triangles
     * form the complete geometric representation of a partition boundary.
     */
    std::vector<Triangle> triangles = {};
};
const static inline std::string m_PartitionShapeManifest = "PartitionShapeManifest";

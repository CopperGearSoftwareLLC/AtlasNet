#pragma once
#include "pch.hpp"
#include "Misc/GeometryUtils.hpp"
#include "GridCell.hpp"


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

    static Shape FromString(const std::string& str);

    /**
     * @brief Check if a point is inside this shape (any triangle)
     */
    inline bool contains(const vec2& point) const
    {
        for (const auto& t : triangles)
        {
            if (GeometryUtils::PointInTriangle(point, t[0], t[1], t[2]))
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Compute closest point on this shape to a given point (across triangles)
     */
    inline vec2 closestPoint(const vec2& point) const
    {
        float bestDist2 = std::numeric_limits<float>::infinity();
        vec2 best = point;
        for (const auto& t : triangles)
        {
            vec2 c = GeometryUtils::ClosestPointOnTriangle(point, t[0], t[1], t[2]);
            float d2 = (point.x - c.x) * (point.x - c.x) + (point.y - c.y) * (point.y - c.y);
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                best = c;
            }
        }
        return best;
    }

    /**
     * @brief Append grid cells derived from triangle bounds to a GridShape
     */
    inline void appendGridCellsFromTriangles(GridShape& gridShape, size_t shapeIndex) const
    {
        for (const auto& triangle : triangles)
        {
            float minX = std::min({triangle[0].x, triangle[1].x, triangle[2].x});
            float maxX = std::max({triangle[0].x, triangle[1].x, triangle[2].x});
            float minY = std::min({triangle[0].y, triangle[1].y, triangle[2].y});
            float maxY = std::max({triangle[0].y, triangle[1].y, triangle[2].y});
            GridCell cell(vec2{minX, minY}, vec2{maxX, maxY}, static_cast<int>(shapeIndex / 2), static_cast<int>(shapeIndex % 2));
            gridShape.addCell(cell);
        }
    }
};
const static inline std::string m_PartitionShapeManifest = "PartitionShapeManifest";

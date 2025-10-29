#ifndef GEOMETRY_UTILS_HPP
#define GEOMETRY_UTILS_HPP

#include "pch.hpp"

namespace GeometryUtils
{
    /**
     * @brief Check if a point is inside a triangle using barycentric coordinates
     * @param p Point to test
     * @param a First vertex of triangle
     * @param b Second vertex of triangle
     * @param c Third vertex of triangle
     * @return true if point is inside triangle, false otherwise
     */
    inline bool PointInTriangle(const vec2& p, const vec2& a, const vec2& b, const vec2& c)
    {
        const float EPSILON = 1e-6f; // Small tolerance for boundary cases
        
        float v0x = c.x - a.x; float v0y = c.y - a.y;
        float v1x = b.x - a.x; float v1y = b.y - a.y;
        float v2x = p.x - a.x; float v2y = p.y - a.y;
        float dot00 = v0x * v0x + v0y * v0y;
        float dot01 = v0x * v1x + v0y * v1y;
        float dot02 = v0x * v2x + v0y * v2y;
        float dot11 = v1x * v1x + v1y * v1y;
        float dot12 = v1x * v2x + v1y * v2y;
        float denom = dot00 * dot11 - dot01 * dot01;
        if (std::abs(denom) < EPSILON) return false;
        float u = (dot11 * dot02 - dot01 * dot12) / denom;
        float v = (dot00 * dot12 - dot01 * dot02) / denom;
        return (u >= -EPSILON) && (v >= -EPSILON) && (u + v <= 1.0f + EPSILON);
    }

    /**
     * @brief Find the closest point on a triangle to a given point
     * @param p Point to project
     * @param a First vertex of triangle
     * @param b Second vertex of triangle
     * @param c Third vertex of triangle
     * @return Closest point on triangle
     */
    inline vec2 ClosestPointOnTriangle(const vec2& p, const vec2& a, const vec2& b, const vec2& c)
    {
        // Project point onto each edge and take closest one
        auto projectOnSegment = [](const vec2& p, const vec2& a, const vec2& b) {
            vec2 ap{p.x - a.x, p.y - a.y};
            vec2 ab{b.x - a.x, b.y - a.y};
            float t = std::max(0.0f, std::min(1.0f, 
                (ap.x * ab.x + ap.y * ab.y) / (ab.x * ab.x + ab.y * ab.y)));
            return vec2{a.x + t * ab.x, a.y + t * ab.y};
        };
        
        vec2 pab = projectOnSegment(p, a, b);
        vec2 pbc = projectOnSegment(p, b, c);
        vec2 pca = projectOnSegment(p, c, a);
        
        float dab = (p.x - pab.x) * (p.x - pab.x) + (p.y - pab.y) * (p.y - pab.y);
        float dbc = (p.x - pbc.x) * (p.x - pbc.x) + (p.y - pbc.y) * (p.y - pbc.y);
        float dca = (p.x - pca.x) * (p.x - pca.x) + (p.y - pca.y) * (p.y - pca.y);
        
        if (dab <= dbc && dab <= dca) return pab;
        if (dbc <= dab && dbc <= dca) return pbc;
        return pca;
    }

    /**
     * @brief Check if a point is within valid coordinate bounds
     * @param p Point to check
     * @param minBound Minimum bound (default 0.0)
     * @param maxBound Maximum bound (default 1.0)
     * @return true if point is within bounds, false otherwise
     */
    inline bool IsWithinBounds(const vec2& p, float minBound = 0.0f, float maxBound = 1.0f)
    {
        return p.x >= minBound && p.x <= maxBound && p.y >= minBound && p.y <= maxBound;
    }

    /**
     * @brief Calculate bounding box for a set of triangles
     * @param triangles Vector of triangles
     * @return Pair of (min, max) bounds
     */
    inline std::pair<vec2, vec2> CalculateBoundingBox(const std::vector<Triangle>& triangles)
    {
        if (triangles.empty()) {
            return {vec2{0, 0}, vec2{0, 0}};
        }

        float minX = std::numeric_limits<float>::infinity();
        float minY = std::numeric_limits<float>::infinity();
        float maxX = -std::numeric_limits<float>::infinity();
        float maxY = -std::numeric_limits<float>::infinity();

        for (const auto& triangle : triangles) {
            for (const auto& vertex : triangle) {
                minX = std::min(minX, vertex.x);
                minY = std::min(minY, vertex.y);
                maxX = std::max(maxX, vertex.x);
                maxY = std::max(maxY, vertex.y);
            }
        }

        return {vec2{minX, minY}, vec2{maxX, maxY}};
    }
}

#endif // GEOMETRY_UTILS_HPP

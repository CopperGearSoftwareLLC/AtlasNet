#pragma once
#include "pch.hpp"

/**
 * @brief Represents a rectangular grid cell for spatial partitioning.
 * 
 * Grid cells are much more efficient than triangles for grid-based partitioning.
 * They provide simple bounds checking and are easier to serialize/deserialize.
 */
struct GridCell {
    /**
     * @brief Bottom-left corner of the cell
     */
    vec2 min;
    
    /**
     * @brief Top-right corner of the cell  
     */
    vec2 max;
    
    /**
     * @brief Grid coordinates (row, col) for this cell
     */
    int row, col;
    
    GridCell() = default;
    
    GridCell(vec2 minCorner, vec2 maxCorner, int r = 0, int c = 0) 
        : min(minCorner), max(maxCorner), row(r), col(c) {}
    
    /**
     * @brief Check if a point is inside this grid cell
     */
    bool contains(const vec2& point) const {
        return point.x >= min.x && point.x <= max.x && 
               point.y >= min.y && point.y <= max.y;
    }
    
    /**
     * @brief Get the center point of this cell
     */
    vec2 getCenter() const {
        return vec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    }
    
    /**
     * @brief Get the width of this cell
     */
    float getWidth() const {
        return max.x - min.x;
    }
    
    /**
     * @brief Get the height of this cell
     */
    float getHeight() const {
        return max.y - min.y;
    }
};

/**
 * @brief Represents a partition shape using grid cells instead of triangles.
 * 
 * This is much more efficient for grid-based partitioning as it eliminates
 * the need for complex triangle-based point-in-shape calculations.
 */
struct GridShape {
    /**
     * @brief Collection of grid cells that form this partition
     */
    std::vector<GridCell> cells;
    
    /**
     * @brief Partition ID for this shape
     */
    std::string partitionId;
    
    GridShape() = default;
    GridShape(const std::string& id) : partitionId(id) {}
    
    /**
     * @brief Check if a point is inside any cell of this shape
     */
    bool contains(const vec2& point) const {
        for (const auto& cell : cells) {
            if (cell.contains(point)) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * @brief Get the total number of cells in this shape
     */
    size_t getCellCount() const {
        return cells.size();
    }
    
    /**
     * @brief Add a cell to this shape
     */
    void addCell(const GridCell& cell) {
        cells.push_back(cell);
    }
    
    /**
     * @brief Get the bounding box of all cells in this shape
     */
    std::pair<vec2, vec2> getBounds() const {
        if (cells.empty()) {
            return {vec2(0, 0), vec2(0, 0)};
        }
        
        vec2 minBounds = cells[0].min;
        vec2 maxBounds = cells[0].max;
        
        for (const auto& cell : cells) {
            minBounds.x = std::min(minBounds.x, cell.min.x);
            minBounds.y = std::min(minBounds.y, cell.min.y);
            maxBounds.x = std::max(maxBounds.x, cell.max.x);
            maxBounds.y = std::max(maxBounds.y, cell.max.y);
        }
        
        return {minBounds, maxBounds};
    }
};

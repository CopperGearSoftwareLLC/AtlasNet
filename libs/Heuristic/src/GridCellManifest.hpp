#pragma once
#include "pch.hpp"

#include "GridCell.hpp"
#include "InternalDB.hpp"
/**
 * @brief Database manifest for grid cell-based partition shapes
 * 
 * This is much more efficient than the triangle-based ShapeManifest
 * for grid-based partitioning systems.
 */
class GridCellManifest {
public:
    static inline const std::string HASH_KEY = "PartitionGridCellManifest";
    static inline const std::string METADATA_KEY = "grid_cell_manifest_metadata";

    /**
     * @brief Store a grid shape for a partition
     */
    static bool Store(const std::string& partitionId, int shapeId, const GridShape& gridShape) {


        std::string shapeData;
        shapeData += "shape_id:" + std::to_string(shapeId) + ";";
        shapeData += "partition_id:" + partitionId + ";";
        shapeData += "grid_cells:";

        for (const auto& cell : gridShape.cells) {
            shapeData += "cell:";
            shapeData += "min(" + std::to_string(cell.min.x) + "," + std::to_string(cell.min.y) + ");";
            shapeData += "max(" + std::to_string(cell.max.x) + "," + std::to_string(cell.max.y) + ");";
            shapeData += "row:" + std::to_string(cell.row) + ";";
            shapeData += "col:" + std::to_string(cell.col) + ";";
        }

        return InternalDB::Get()->HSet(HASH_KEY, partitionId, shapeData);
    }

    /**
     * @brief Fetch a grid shape for a partition
     */
    static std::optional<std::string> Fetch( const std::string& partitionId) {
        
        
        std::string data = InternalDB::Get()->HGet(HASH_KEY, partitionId).value();
        if (data.empty()) return std::nullopt;
        
        return data;
    }
    static std::unordered_map<std::string,std::string> FetchAll()
    {
        return InternalDB::Get()->HGetAll(HASH_KEY);
    }

    /**
     * @brief Parse grid shape data from database string
     */
    static GridShape ParseGridShape(const std::string& data) {
        GridShape gridShape;
        
        // Find the grid_cells section
        size_t cellsStart = data.find("grid_cells:");
        if (cellsStart == std::string::npos) {
            return gridShape;
        }
        
        size_t pos = cellsStart + 11; // Skip "grid_cells:"
        while (pos < data.length()) {
            // Find next cell marker
            pos = data.find("cell:", pos);
            if (pos == std::string::npos) break;
            pos += 5; // Skip "cell:"
            
            GridCell cell;
            bool validCell = true;
            
            // Parse min coordinates
            size_t minStart = data.find("min(", pos);
            if (minStart != std::string::npos) {
                size_t minEnd = data.find(")", minStart);
                if (minEnd != std::string::npos) {
                    size_t comma = data.find(",", minStart);
                    if (comma != std::string::npos && comma < minEnd) {
                        try {
                            cell.min.x = std::stof(data.substr(minStart + 4, comma - (minStart + 4)));
                            cell.min.y = std::stof(data.substr(comma + 1, minEnd - (comma + 1)));
                        } catch (const std::exception& e) {
                            validCell = false;
                        }
                    }
                }
            }
            
            // Parse max coordinates
            size_t maxStart = data.find("max(", pos);
            if (maxStart != std::string::npos) {
                size_t maxEnd = data.find(")", maxStart);
                if (maxEnd != std::string::npos) {
                    size_t comma = data.find(",", maxStart);
                    if (comma != std::string::npos && comma < maxEnd) {
                        try {
                            cell.max.x = std::stof(data.substr(maxStart + 4, comma - (maxStart + 4)));
                            cell.max.y = std::stof(data.substr(comma + 1, maxEnd - (comma + 1)));
                        } catch (const std::exception& e) {
                            validCell = false;
                        }
                    }
                }
            }
            
            // Parse row and col
            size_t rowStart = data.find("row:", pos);
            if (rowStart != std::string::npos) {
                size_t rowEnd = data.find(";", rowStart);
                if (rowEnd != std::string::npos) {
                    try {
                        cell.row = std::stoi(data.substr(rowStart + 4, rowEnd - (rowStart + 4)));
                    } catch (const std::exception& e) {
                        validCell = false;
                    }
                }
            }
            
            size_t colStart = data.find("col:", pos);
            if (colStart != std::string::npos) {
                size_t colEnd = data.find(";", colStart);
                if (colEnd != std::string::npos) {
                    try {
                        cell.col = std::stoi(data.substr(colStart + 4, colEnd - (colStart + 4)));
                    } catch (const std::exception& e) {
                        validCell = false;
                    }
                }
            }
            
            if (validCell) {
                gridShape.addCell(cell);
            }
            
            pos = std::max({minStart, maxStart, rowStart, colStart}) + 1;
        }
        
        return gridShape;
    }

    /**
     * @brief Store metadata about the grid cell assignments
     */
    static bool StoreMetadata( size_t numShapes, size_t numPartitions) {
      

        std::string metadata = "total_shapes:" + std::to_string(numShapes) +
                             ";total_partitions:" + std::to_string(numPartitions) +
                             ";timestamp:" + std::to_string(std::time(nullptr));

         InternalDB::Get()->Set(METADATA_KEY, metadata);
         return true;
    }

    /**
     * @brief Clear all grid shapes from the manifest
     */
    static bool Clear() {

        return InternalDB::Get()->HDelAll(HASH_KEY);
    }
};

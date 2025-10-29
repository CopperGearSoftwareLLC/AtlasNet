#include "Heuristic.hpp"
#include "GridCell.hpp"

/**
 * @brief Computes partition shapes using heuristic algorithms.
 * 
 * @note This is currently a placeholder implementation that returns a single triangular shape.
 *       This method should be replaced with actual heuristic logic that analyzes system state
 *       and computes optimal partition boundaries.
 * 
 * @return std::vector<Shape> A collection of shapes representing partition boundaries.
 *         Currently returns a single triangle as a demonstration.
 */
std::vector<Shape> Heuristic::computePartition()
{
    HeuristicType theType = Grid;
    switch (theType)
    {
    case BigSquare:
        return computeBigSquareShape();
    case QuadTree:
        // Implement QuadTree heuristic logic here
        break;
    case KDTree:
        // Implement KDTree heuristic logic here
        break;
    case Grid:
        return computeGridShape();
    default:
        // Default case if no heuristic type is matched
        break;  
    }
    return {};
}
//(0,1)(1,1)
//(0,0)(1,0)
std::vector<Shape> Heuristic::computeBigSquareShape()
{
    std::vector<Shape> shapes;
    Shape Square;
    Square.triangles = {{vec2{0, 0}, vec2{1, 0},vec2 {0, 1}} , {vec2{1, 1}, vec2{1, 0}, vec2{0, 1}}};

    shapes.push_back(Square);
    return shapes;
}
std::vector<Shape> Heuristic::computeGridShape()
{
    std::vector<Shape> shapes;
    int gridSize = 2; // Define the grid size (2x2 = 4 partitions)
    float step = 1.0f / gridSize;

    for (int i = 0; i < gridSize; ++i)
    {
        for (int j = 0; j < gridSize; ++j)
        {
            Shape cell;
            float x0 = i * step;
            float y0 = j * step;
            float x1 = (i + 1) * step;
            float y1 = (j + 1) * step;

            // Create two triangles for each grid cell (keeping compatibility with existing system)
            cell.triangles.push_back({vec2{x0, y0}, vec2{x1, y0}, vec2{x0, y1}});
            cell.triangles.push_back({vec2{x1, y1}, vec2{x1, y0}, vec2{x0, y1}});

            // Debug: Log grid cell coordinates
            std::cout << "Grid cell (" << i << "," << j << "): (" << x0 << "," << y0 << ") to (" << x1 << "," << y1 << ")" << std::endl;

            shapes.push_back(cell);
        }
    }

    return shapes;
}

/**
 * @brief Computes partition shapes using grid cells (new efficient approach)
 */
std::vector<GridShape> Heuristic::computeGridCellShapes()
{
    std::vector<GridShape> gridShapes;
    int gridSize = 2; // 2x2 grid = 4 partitions
    float step = 1.0f / gridSize;

    for (int i = 0; i < gridSize; ++i)
    {
        for (int j = 0; j < gridSize; ++j)
        {
            GridShape gridShape;
            gridShape.partitionId = "ePartition grid_" + std::to_string(i) + "_" + std::to_string(j);
            
            float x0 = i * step;
            float y0 = j * step;
            float x1 = (i + 1) * step;
            float y1 = (j + 1) * step;

            // Create a single grid cell instead of two triangles
            GridCell cell(vec2{x0, y0}, vec2{x1, y1}, i, j);
            gridShape.addCell(cell);

            // Debug: Log grid cell coordinates
            std::cout << "Grid cell (" << i << "," << j << "): (" << x0 << "," << y0 << ") to (" << x1 << "," << y1 << ")" << std::endl;

            gridShapes.push_back(gridShape);
        }
    }

    return gridShapes;
}


#include "Heuristic.hpp"
#include "GridCell.hpp"
#include <functional>

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
    HeuristicType theType = QuadTree;
    switch (theType)
    {
    case BigSquare:
        return computeBigSquareShape();
    case QuadTree:
        return computeQuadtreeShape();
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

/**
 * @brief Quadtree node structure for building the quadtree
 */
struct QuadtreeNode {
    vec2 min, max;  // Bounding box
    std::unique_ptr<QuadtreeNode> topLeft;
    std::unique_ptr<QuadtreeNode> topRight;
    std::unique_ptr<QuadtreeNode> bottomLeft;
    std::unique_ptr<QuadtreeNode> bottomRight;
    bool isLeaf;
    int depth;
    int maxDepth;
    
    QuadtreeNode(vec2 minCorner, vec2 maxCorner, int d = 0, int md = 4)
        : min(minCorner), max(maxCorner), isLeaf(true), depth(d), maxDepth(md) {}
    
    /**
     * @brief Subdivide this node into 4 children
     */
    void subdivide() {
        if (!isLeaf || depth >= maxDepth) return;
        
        float midX = (min.x + max.x) * 0.5f;
        float midY = (min.y + max.y) * 0.5f;
        
        bottomLeft = std::make_unique<QuadtreeNode>(
            vec2{min.x, min.y}, vec2{midX, midY}, depth + 1, maxDepth);
        bottomRight = std::make_unique<QuadtreeNode>(
            vec2{midX, min.y}, vec2{max.x, midY}, depth + 1, maxDepth);
        topLeft = std::make_unique<QuadtreeNode>(
            vec2{min.x, midY}, vec2{midX, max.y}, depth + 1, maxDepth);
        topRight = std::make_unique<QuadtreeNode>(
            vec2{midX, midY}, vec2{max.x, max.y}, depth + 1, maxDepth);
        
        isLeaf = false;
    }
    
    /**
     * @brief Collect all leaf nodes as shapes
     */
    void collectLeafShapes(std::vector<Shape>& shapes, int& partitionIndex) {
        if (isLeaf) {
            // Create a shape with two triangles for this leaf node (like grid heuristic)
            Shape cell;
            float x0 = min.x;
            float y0 = min.y;
            float x1 = max.x;
            float y1 = max.y;
            
            // Create two triangles for the rectangle (keeping compatibility with existing system)
            cell.triangles.push_back({vec2{x0, y0}, vec2{x1, y0}, vec2{x0, y1}});
            cell.triangles.push_back({vec2{x1, y1}, vec2{x1, y0}, vec2{x0, y1}});
            
            shapes.push_back(cell);
            partitionIndex++;
        } else {
            // Recursively collect from children
            if (bottomLeft) bottomLeft->collectLeafShapes(shapes, partitionIndex);
            if (bottomRight) bottomRight->collectLeafShapes(shapes, partitionIndex);
            if (topLeft) topLeft->collectLeafShapes(shapes, partitionIndex);
            if (topRight) topRight->collectLeafShapes(shapes, partitionIndex);
        }
    }
};

/**
 * @brief Computes partition shapes using quadtree subdivision
 * 
 * Creates a quadtree that recursively subdivides the space, then converts
 * each leaf node to a Shape (with triangles) for compatibility with the existing system.
 * The quadtree works like the grid heuristic in terms of creating rectangular shapes.
 */
std::vector<Shape> Heuristic::computeQuadtreeShape()
{
    std::vector<Shape> shapes;
    
    // Create root quadtree node covering the entire space [0,1] x [0,1]
    QuadtreeNode root(vec2{0.0f, 0.0f}, vec2{1.0f, 1.0f}, 0, 3); // maxDepth = 3 gives us up to 64 partitions
    
    // Subdivide the quadtree recursively
    // For now, we'll do a simple uniform subdivision
    // In the future, this could be based on entity density
    std::function<void(QuadtreeNode*)> subdivideRecursive = [&](QuadtreeNode* node) {
        if (node->depth < node->maxDepth) {
            node->subdivide();
            if (!node->isLeaf) {
                subdivideRecursive(node->bottomLeft.get());
                subdivideRecursive(node->bottomRight.get());
                subdivideRecursive(node->topLeft.get());
                subdivideRecursive(node->topRight.get());
            }
        }
    };
    
    subdivideRecursive(&root);
    
    // Collect all leaf nodes as shapes
    int partitionIndex = 0;
    root.collectLeafShapes(shapes, partitionIndex);
    
    std::cout << "Quadtree heuristic: Created " << shapes.size() << " partition shapes" << std::endl;
    
    return shapes;
}


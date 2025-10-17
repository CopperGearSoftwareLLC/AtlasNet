#include "Heuristic.hpp"

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
    HeuristicType theType = BigSquare;
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
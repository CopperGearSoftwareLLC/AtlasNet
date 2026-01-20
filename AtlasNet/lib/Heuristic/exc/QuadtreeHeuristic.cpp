#include "QuadtreeHeuristic.hpp"
#include <iostream>
#include <algorithm>
#include <functional>

void QuadtreeNode::subdivide() {
    if (!isLeaf || depth >= maxDepth) {
        return;
    }
    
    // Calculate midpoint for subdivision
    float midX = (min.x + max.x) * 0.5f;
    float midY = (min.y + max.y) * 0.5f;
    
    // Create four child quadrants
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

void QuadtreeNode::collectLeafShapes(std::vector<Shape>& shapes, int& partitionIndex) {
    if (isLeaf) {
        // Convert leaf node to a Shape with two triangles (for compatibility with existing system)
        Shape cell;
        const float x0 = min.x;
        const float y0 = min.y;
        const float x1 = max.x;
        const float y1 = max.y;
        
        // Create two triangles forming a rectangle
        // Triangle 1: bottom-left, bottom-right, top-left
        cell.triangles.push_back({vec2{x0, y0}, vec2{x1, y0}, vec2{x0, y1}});
        // Triangle 2: top-right, bottom-right, top-left
        cell.triangles.push_back({vec2{x1, y1}, vec2{x1, y0}, vec2{x0, y1}});
        
        shapes.push_back(cell);
        partitionIndex++;
    } else {
        // Recursively collect shapes from all children
        if (bottomLeft) bottomLeft->collectLeafShapes(shapes, partitionIndex);
        if (bottomRight) bottomRight->collectLeafShapes(shapes, partitionIndex);
        if (topLeft) topLeft->collectLeafShapes(shapes, partitionIndex);
        if (topRight) topRight->collectLeafShapes(shapes, partitionIndex);
    }
}

std::vector<Shape> QuadtreeHeuristic::computeQuadtreeShape() {
    std::vector<Shape> shapes;
    
    // Create root quadtree node covering the entire space [0,1] x [0,1]
    // maxDepth = 3 creates 4^3 = 64 partitions
    const int maxDepth = 3;
    QuadtreeNode root(vec2{0.0f, 0.0f}, vec2{1.0f, 1.0f}, 0, maxDepth);
    
    // Uniform subdivision: recursively subdivide all nodes to max depth
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
    
    // Collect all leaf nodes and convert them to shapes
    int partitionIndex = 0;
    root.collectLeafShapes(shapes, partitionIndex);
    
    std::cout << "Quadtree heuristic: Created " << shapes.size() << " partition shapes" << std::endl;
    
    return shapes;
}

std::vector<Shape> QuadtreeHeuristic::computeQuadtreeShapeDensity(
    const std::vector<AtlasEntity>& entities,
    int maxDepth,
    int minEntitiesPerNode)
{
    std::vector<Shape> shapes;
    
    if (entities.empty()) {
        // Fallback to uniform subdivision if no entities provided
        return computeQuadtreeShape();
    }
    
    // Create root quadtree node covering the entire space [0,1] x [0,1]
    QuadtreeNode root(vec2{0.0f, 0.0f}, vec2{1.0f, 1.0f}, 0, maxDepth);
    
    // Count entities in root node
    root.countEntities(entities);
    
    // Adaptive subdivision: only subdivide nodes with high entity density
    std::function<void(QuadtreeNode*)> subdivideRecursive = [&](QuadtreeNode* node) {
        // Subdivide if:
        // 1. We haven't reached max depth
        // 2. The node contains more entities than the threshold
        if (node->depth < node->maxDepth && node->entityCount > minEntitiesPerNode) {
            node->subdivide();
            if (!node->isLeaf) {
                // Count entities in each child quadrant
                node->bottomLeft->countEntities(entities);
                node->bottomRight->countEntities(entities);
                node->topLeft->countEntities(entities);
                node->topRight->countEntities(entities);
                
                // Recursively subdivide children that meet the criteria
                subdivideRecursive(node->bottomLeft.get());
                subdivideRecursive(node->bottomRight.get());
                subdivideRecursive(node->topLeft.get());
                subdivideRecursive(node->topRight.get());
            }
        }
    };
    
    subdivideRecursive(&root);
    
    // Collect all leaf nodes and convert them to shapes
    int partitionIndex = 0;
    root.collectLeafShapes(shapes, partitionIndex);
    
    std::cout << "Quadtree heuristic (density-based): Created " << shapes.size() 
              << " partition shapes from " << entities.size() << " entities" << std::endl;
    
    return shapes;
}


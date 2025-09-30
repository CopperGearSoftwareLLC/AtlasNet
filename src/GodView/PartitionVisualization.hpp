#pragma once
#include "pch.hpp"
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <map>
#include <algorithm>
#include <limits>
#include <cmath>

struct Entity {
    float x, y;
    float velocityX, velocityY;
    int id;
    int partitionId;
    ImVec4 color;
    bool active = true;
};

// Quadtree node structure
struct QuadtreeNode {
    float minX, minY, maxX, maxY;
    std::vector<Entity*> entities;
    std::unique_ptr<QuadtreeNode> topLeft;
    std::unique_ptr<QuadtreeNode> topRight;
    std::unique_ptr<QuadtreeNode> bottomLeft;
    std::unique_ptr<QuadtreeNode> bottomRight;
    bool isLeaf;
    int depth;
    int maxDepth;
    int maxEntities;
    
    QuadtreeNode(float x1, float y1, float x2, float y2, int d = 0, int md = 4, int me = 4)
        : minX(x1), minY(y1), maxX(x2), maxY(y2), isLeaf(true), depth(d), maxDepth(md), maxEntities(me) {}
};

// KD-tree node structure
struct KdTreeNode {
    float minX, minY, maxX, maxY;
    std::vector<Entity*> entities;
    std::unique_ptr<KdTreeNode> left;
    std::unique_ptr<KdTreeNode> right;
    bool isLeaf;
    int depth;
    int maxDepth;
    int maxEntities;
    int splitAxis; // 0 = X-axis, 1 = Y-axis
    float splitValue; // The value where the split occurs
    
    KdTreeNode(float x1, float y1, float x2, float y2, int d = 0, int md = 8, int me = 4)
        : minX(x1), minY(y1), maxX(x2), maxY(y2), isLeaf(true), depth(d), maxDepth(md), maxEntities(me), splitAxis(0), splitValue(0.0f) {}
};

// Partitioning method enum
enum class PartitioningMethod {
    QUADTREE,
    KD_TREE
};

// Partition visualization method enum
enum class PartitionVisualizationMethod {
    QUADTREE,
    K_MEANS,
    DBSCAN
};

// Clustering method enum
enum class ClusteringMethod {
    NONE,
    K_MEANS,
    DBSCAN
};

struct Partition {
    int id;
    std::string host;
    std::vector<Entity> entities;
    ImVec4 color;
    float minX, minY, maxX, maxY; // Bounding box
};

// K-means cluster structure
struct Cluster {
    float centroidX, centroidY;
    ImVec4 color;
    std::vector<int> entityIndices;
    int clusterId;
};

// DBSCAN cluster structure
struct DBSCANCluster {
    ImVec4 color;
    std::vector<int> entityIndices;
    int clusterId;
};

class PartitionVisualization {
public:
    PartitionVisualization();
    ~PartitionVisualization() = default;

    void update(float dt);
    void render();

    // Data management
    void generateTestData();
    void clearData();
    void addRandomEntities(int count = 10);
    void addEntityAt(float x, float y);
    void handleEntityClick(float clickX, float clickY);
    void generateConcentricCirclesDataset();
    void generateConcentricCirclesDatasetLight();
    void regeneratePartitions() = delete; // legacy disabled

    // Partition management
    void createPartition(int id, const std::string& host, const ImVec4& color) = delete; // legacy disabled
    // (legacy assign kept removed; quadtree collection assigns members)
    
    // Partitioning functionality
    void generatePartitions();
    
    // Quadtree functionality
    void generateQuadtreePartitions();
    void buildQuadtree();
    void insertEntityIntoQuadtree(Entity* entity, QuadtreeNode* node);
    void subdivideNode(QuadtreeNode* node);
    void collectQuadtreePartitions(QuadtreeNode* node);
    void clearQuadtree();
    
    // KD-tree functionality
    void generateKdTreePartitions();
    void buildKdTree();
    void insertEntityIntoKdTree(Entity* entity, KdTreeNode* node);
    void subdivideKdTreeNode(KdTreeNode* node);
    void collectKdTreePartitions(KdTreeNode* node);
    void clearKdTree();
    void renderKdTreeLines();
    void drawKdTreeNode(KdTreeNode* node, const ImVec4& color, float thickness);
    
    // Clustering functionality
    void performKMeansClustering();
    void performDBSCANClustering();
    void clearClustering();
    
    // DBSCAN functionality
    void expandDBSCANCluster(int entityIndex, int clusterId);
    std::vector<int> getDBSCANNeighbors(int entityIndex);
    void updateDBSCANEntityColors();
    void resetDBSCANLabels();
    ImVec4 getDBSCANClusterColor(int clusterIndex);
    void renderDBSCANClusters();
    void calculateDBSCANVoronoiCell(int clusterId, std::vector<ImVec2>& cell);
    void updateDBSCANIfNeeded(float dt);
    
    // Getters
    const std::vector<Partition>& getPartitions() const { return m_partitions; }
    const std::vector<Entity>& getAllEntities() const { return m_allEntities; }
    const std::vector<Cluster>& getClusters() const { return m_clusters; }
    int getTotalEntityCount() const { return m_allEntities.size(); }

private:
    // Data
    std::vector<Partition> m_partitions;
    std::vector<Entity> m_allEntities;
    std::vector<Cluster> m_clusters;
    std::vector<DBSCANCluster> m_dbscanClusters;
    std::vector<int> m_dbscanEntityLabels; // -1=unvisited, -2=noise, >=0=clusterId
    
    // Partitioning method
    PartitioningMethod m_currentMethod = PartitioningMethod::QUADTREE;
    
    // Clustering method
    ClusteringMethod m_currentClusteringMethod = ClusteringMethod::NONE;
    
    // Partition visualization method
    PartitionVisualizationMethod m_partitionVisualizationMethod = PartitionVisualizationMethod::QUADTREE;
    
    // Quadtree
    std::unique_ptr<QuadtreeNode> m_quadtree;
    float m_worldMinX = -400.0f, m_worldMaxX = 400.0f;
    float m_worldMinY = -300.0f, m_worldMaxY = 300.0f;
    
    // KD-tree
    std::unique_ptr<KdTreeNode> m_kdtree;
    
    // UI state
    bool m_showGlobalView = true;
    bool m_showDetailedView = true;
    bool m_autoRegenerate = false;
    float m_regenerateTimer = 0.0f;
    const float m_regenerateInterval = 2.0f;
    
    // Movement settings
    bool m_entitiesMoving = true;
    float m_entitySpeed = 50.0f;
    float m_movementUpdateTimer = 0.0f;
    const float m_movementUpdateInterval = 0.1f; // Update partitions every 0.1 seconds
    
    // Visualization settings
    float m_entitySize = 8.0f;
    bool m_showQuadtreeLines = true;
    bool m_showKdTreeLines = true;
    bool m_useVoronoi = false; // Toggle between convex hulls and Voronoi
    bool m_spawnWithRightClick = false; // Toggle for RMB spawning
    int m_spawnCountPerClick = 1;
    
    // K-means settings
    int m_kMeansK = 3;
    int m_kMeansMaxIterations = 100;
    float m_kMeansConvergenceThreshold = 0.01f;
    float m_kMeansUpdateTimer = 0.0f;
    float m_kMeansUpdateInterval = 0.5f; // Update clustering every 0.5 seconds
    
    // DBSCAN settings
    float m_dbscanEps = 50.0f;
    int m_dbscanMinPts = 3;
    float m_dbscanUpdateTimer = 0.0f;
    float m_dbscanUpdateInterval = 0.5f;
    
    // Random generation
    std::random_device m_rd;
    std::mt19937 m_gen;
    std::uniform_real_distribution<float> m_posDist;
    std::uniform_real_distribution<float> m_colorDist;
    
    // Rendering helpers
    void renderGlobalView();
    void renderDetailedView();
    void renderEntity(const Entity& entity);
    void renderQuadtreeLines();
    void drawQuadtreeNode(QuadtreeNode* node, const ImVec4& color, float thickness);
    void renderQuadtreeSquares(QuadtreeNode* node);
    ImVec4 getPartitionColor(int partitionId);
    ImVec4 getEntityColor(int entityId);
    
    // Movement and collision
    void updateEntityMovement(float dt);
    void handleEntityCollision(Entity& entity);
    void updatePartitionsIfNeeded(float dt);
    
    // K-means helper methods
    void initializeKMeansCentroids();
    void assignEntitiesToClusters();
    void updateClusterCentroids();
    void updateEntityColorsFromClusters();
    void renderClusterCentroids();
    void renderClusterLines();
    void renderConvexHulls();
    void renderVoronoiCells();
    void calculateConvexHull(const std::vector<Entity*>& entities, std::vector<ImVec2>& hull);
    void calculateVoronoiCell(int clusterIndex, std::vector<ImVec2>& cell);
    ImVec4 getClusterColor(int clusterIndex);
    
    // Voronoi helpers
    struct HalfPlane { 
        ImVec2 n; 
        float d; 
    };
    std::vector<ImVec2> clipPolygonWithHalfPlane(const std::vector<ImVec2>& poly, const HalfPlane& hp);
    std::vector<ImVec2> computeVoronoiCell(const ImVec2& site, const std::vector<ImVec2>& allSites, const ImVec2& boundsCenter, const ImVec2& boundsSize);
    
    // Partition visualization helpers
    void calculatePartitionConvexHull(const Partition& partition, std::vector<ImVec2>& hull);
    void calculatePartitionVoronoiCell(const Partition& partition, std::vector<ImVec2>& cell);
    
    // DBSCAN constants
    static constexpr int DBSCAN_UNVISITED = -1;
    static constexpr int DBSCAN_NOISE = -2;
    float calculateDistance(float x1, float y1, float x2, float y2);
    bool hasKMeansConverged();
    
    // Quadtree-based K-means methods
    void initializeKMeansCentroidsFromQuadtree();
    void assignEntitiesToClustersUsingQuadtree();
    void updateKMeansIfNeeded(float dt);
    void updateClusterCentroidsFromQuadtree();
    void collectAllEntitiesFromQuadtree(QuadtreeNode* node, std::vector<Entity*>& entities);
    Entity* findEntityInQuadtree(QuadtreeNode* node, int entityId);
};

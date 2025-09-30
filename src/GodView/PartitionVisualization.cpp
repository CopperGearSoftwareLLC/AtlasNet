#include "PartitionVisualization.hpp"
#include <map>
#include <algorithm>
#include <limits>
#include <cmath>

PartitionVisualization::PartitionVisualization() 
    : m_gen(m_rd()), m_posDist(-300.0f, 300.0f), m_colorDist(0.0f, 1.0f) {
    generateTestData();
}

void PartitionVisualization::update(float dt) {
    // Update entity movement
    if (m_entitiesMoving) {
        updateEntityMovement(dt);
        updatePartitionsIfNeeded(dt);
    }
    
    // Update clustering if enabled
    if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
        updateKMeansIfNeeded(dt);
    } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
        updateDBSCANIfNeeded(dt);
    }
    
    if (m_autoRegenerate) {
        m_regenerateTimer += dt;
        if (m_regenerateTimer >= m_regenerateInterval) {
            generatePartitions();
            m_regenerateTimer = 0.0f;
        }
    }
}

void PartitionVisualization::render() {
    if (!m_showGlobalView && !m_showDetailedView) return;

    const auto availableSpace = ImGui::GetContentRegionAvail();
    
    // Main layout - split into two panels
    if (m_showGlobalView && m_showDetailedView) {
        // Two-panel layout
        ImGui::BeginChild("GlobalView", ImVec2(availableSpace.x * (2.0f / 3.0f), 0), 
                         ImGuiChildFlags_ResizeX | ImGuiChildFlags_Borders);
        renderGlobalView();
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("DetailedView");
        renderDetailedView();
        ImGui::EndChild();
    } else if (m_showGlobalView) {
        renderGlobalView();
    } else if (m_showDetailedView) {
        renderDetailedView();
    }
}

void PartitionVisualization::renderGlobalView() {
    ImGui::Text("Global Partition View");
    ImGui::Separator();
    
    // Control panel
    ImGui::Checkbox("Auto Regenerate", &m_autoRegenerate);
    ImGui::SameLine();
    if (ImGui::Button("Rebuild Partitions")) {
        generatePartitions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Entities")) {
        addRandomEntities(5);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
        clearData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn Circles Dataset")) {
        generateConcentricCirclesDataset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Spawn Circles Dataset (Light)")) {
        generateConcentricCirclesDatasetLight();
    }
    
    // Partitioning method selector
    ImGui::Separator();
    ImGui::Text("Partitioning Method:");
    const char* methods[] = { "Quadtree", "KD-Tree" };
    int currentMethod = static_cast<int>(m_currentMethod);
    if (ImGui::Combo("##Method", &currentMethod, methods, 2)) {
        m_currentMethod = static_cast<PartitioningMethod>(currentMethod);
        generatePartitions(); // Regenerate with new method
    }
    
    // Clustering method selector
    ImGui::Text("Clustering Method:");
    const char* clusteringMethods[] = { "None", "K-Means", "DBSCAN" };
    int currentClusteringMethod = static_cast<int>(m_currentClusteringMethod);
    if (ImGui::Combo("##ClusteringMethod", &currentClusteringMethod, clusteringMethods, 3)) {
        m_currentClusteringMethod = static_cast<ClusteringMethod>(currentClusteringMethod);
        if (m_currentClusteringMethod == ClusteringMethod::NONE) {
            clearClustering();
        } else if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
            performKMeansClustering();
        } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
            performDBSCANClustering();
        }
    }
    
    
    ImGui::Checkbox("Show Quadtree Lines", &m_showQuadtreeLines);
    ImGui::SameLine();
    ImGui::Checkbox("Show KD-Tree Lines", &m_showKdTreeLines);
    
    // K-means controls
    if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
        ImGui::Separator();
        ImGui::Text("K-Means Settings:");
        if (ImGui::SliderInt("K Value", &m_kMeansK, 2, 8)) {
            // K value changed, regenerate clustering
            performKMeansClustering();
        }
        
        if (ImGui::SliderInt("Max Iterations", &m_kMeansMaxIterations, 10, 200)) {
            // Max iterations changed, regenerate clustering
            performKMeansClustering();
        }
        if (ImGui::SliderFloat("Convergence Threshold", &m_kMeansConvergenceThreshold, 0.001f, 0.1f, "%.3f")) {
            // Convergence threshold changed, regenerate clustering
            performKMeansClustering();
        }
        
        if (ImGui::Button("Run K-Means")) {
            performKMeansClustering();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Clusters")) {
            clearClustering();
        }
        
        ImGui::SliderFloat("Update Interval", &m_kMeansUpdateInterval, 0.1f, 2.0f, "%.1f s");
        
        ImGui::Checkbox("Use Voronoi", &m_useVoronoi);
    }
    
    // DBSCAN controls
    if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
        ImGui::Separator();
        ImGui::Text("DBSCAN Settings:");
        if (ImGui::SliderFloat("Epsilon", &m_dbscanEps, 10.0f, 200.0f)) {
            // Epsilon changed, regenerate clustering
            performDBSCANClustering();
        }
        
        if (ImGui::SliderInt("Min Points", &m_dbscanMinPts, 2, 10)) {
            // Min points changed, regenerate clustering
            performDBSCANClustering();
        }
        
        if (ImGui::Button("Run DBSCAN")) {
            performDBSCANClustering();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Clusters")) {
            clearClustering();
        }
        
        ImGui::SliderFloat("Update Interval", &m_dbscanUpdateInterval, 0.1f, 2.0f, "%.1f s");
        
        ImGui::Checkbox("Use Voronoi", &m_useVoronoi);
    }
    
    ImGui::Separator();
    ImGui::Checkbox("Entities Moving", &m_entitiesMoving);
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_entitySpeed, 10.0f, 200.0f);
    
    if (m_entitiesMoving) {
        ImGui::Text("Left-click entities to give them random velocity");
    }
    
    // RMB spawn controls
    ImGui::Checkbox("Spawn With Right Click", &m_spawnWithRightClick);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputInt("Count per click", &m_spawnCountPerClick)) {
        if (m_spawnCountPerClick < 1) m_spawnCountPerClick = 1;
        if (m_spawnCountPerClick > 1000) m_spawnCountPerClick = 1000;
    }
    
    ImGui::Separator();
    
    // ImPlot visualization
    if (ImPlot::BeginPlot("Partition Map", ImVec2(-1, -1), 
                         ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
        
        // Set up plot
        ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxesLimits(-400, 400, -300, 300);
        
        // Render quadtree grid lines
        if (m_showQuadtreeLines && m_quadtree && m_currentMethod == PartitioningMethod::QUADTREE) {
            renderQuadtreeLines();
        }
        
        // Render KD-tree grid lines
        if (m_showKdTreeLines && m_kdtree && m_currentMethod == PartitioningMethod::KD_TREE) {
            renderKdTreeLines();
        }
        
        // Render entities
        for (const auto& entity : m_allEntities) {
            renderEntity(entity);
        }
        
        // Handle RMB spawning inside the plot
        if (m_spawnWithRightClick && ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImPlotPoint mp = ImPlot::GetPlotMousePos();
            std::normal_distribution<float> jitter(0.0f, 2.0f);
            int toSpawn = m_spawnCountPerClick;
            for (int i = 0; i < toSpawn; ++i) {
                float sx = static_cast<float>(mp.x);
                float sy = static_cast<float>(mp.y);
                if (toSpawn > 1) {
                    sx += jitter(m_gen);
                    sy += jitter(m_gen);
                }
                addEntityAt(sx, sy);
            }
            generatePartitions();
        }
        
        // Handle left-click on entities to make them move
        if (ImPlot::IsPlotHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImPlotPoint mp = ImPlot::GetPlotMousePos();
            handleEntityClick(static_cast<float>(mp.x), static_cast<float>(mp.y));
        }
        
        // Render based on partition method
        if (m_partitionVisualizationMethod == PartitionVisualizationMethod::K_MEANS) {
            // Render clustering visualization
            if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
                renderClusterCentroids();
                renderClusterLines();
                if (m_useVoronoi) {
                    renderVoronoiCells();
                } else {
                    renderConvexHulls();
                }
            } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
                renderDBSCANClusters();
            }
        } else {
            // Render quadtree visualization 
            if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
                renderClusterCentroids();
                renderClusterLines();
            } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
                renderDBSCANClusters();
            }
        }
        
        ImPlot::EndPlot();
    }
}

void PartitionVisualization::renderDetailedView() {
    ImGui::Text("Partition Tiles");
    
    // Auto-determine display method based on left panel selection
    if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
        m_partitionVisualizationMethod = PartitionVisualizationMethod::K_MEANS;
    } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
        m_partitionVisualizationMethod = PartitionVisualizationMethod::DBSCAN;
    } else {
        // When no clustering, mirror the selected partitioning method
        m_partitionVisualizationMethod = (m_currentMethod == PartitioningMethod::KD_TREE)
            ? PartitionVisualizationMethod::DBSCAN /* placeholder for non-quadtree */
            : PartitionVisualizationMethod::QUADTREE;
    }
    
    ImGui::Separator();
    
    // Calculate tile size based on available space
    const float tileWidth = 200.0f;
    const float tileHeight = 180.0f;
    const float spacing = 10.0f;
    
    // Get available width for calculating columns
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columns = static_cast<int>(availableWidth / (tileWidth + spacing));
    const int actualColumns = std::max(1, columns);
    
    int currentColumn = 0;
    
    // Check if we should show DBSCAN clusters (regardless of partition method)
    bool showDBSCANClusters = (m_currentClusteringMethod == ClusteringMethod::DBSCAN && !m_dbscanClusters.empty());
    
    // Render based on partition method
    if (showDBSCANClusters) {
        // Render DBSCAN clusters in tiled grid
        for (const auto& cluster : m_dbscanClusters) {
            if (cluster.entityIndices.empty()) continue;

            if (currentColumn == 0) {
                ImGui::BeginGroup();
            }

            ImGui::BeginChild(("DBSCANClusterTile_" + std::to_string(cluster.clusterId)).c_str(),
                             ImVec2(tileWidth, tileHeight),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

            // Header with cluster color
            ImGui::PushStyleColor(ImGuiCol_Header, cluster.color);
            ImGui::Selectable(("D" + std::to_string(cluster.clusterId)).c_str(), false,
                             ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor();

            // Stats
            ImGui::Text("Members: %zu", cluster.entityIndices.size());
            ImGui::Text("Epsilon: %.1f", m_dbscanEps);

            if (ImPlot::BeginPlot(("DBSCANCluster_" + std::to_string(cluster.clusterId) + "_Tile").c_str(),
                                 ImVec2(-1, 100), ImPlotFlags_CanvasOnly | ImPlotFlags_Equal)) {

                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);

                // Calculate cluster bounds for auto-centering and zooming
                float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
                float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();
                
                for (int entityId : cluster.entityIndices) {
                    for (const auto& e : m_allEntities) {
                        if (e.id == entityId && e.active) {
                            minX = std::min(minX, e.x);
                            maxX = std::max(maxX, e.x);
                            minY = std::min(minY, e.y);
                            maxY = std::max(maxY, e.y);
                            break;
                        }
                    }
                }
                
                // Add padding and set bounds to fit the cluster
                float padding = std::max(maxX - minX, maxY - minY) * 0.2f; // 20% padding
                if (padding < 20.0f) padding = 20.0f; // Minimum padding
                
                ImPlot::SetupAxesLimits(
                    minX - padding, maxX + padding,
                    minY - padding, maxY + padding
                );

                // Draw entities in this cluster
                for (int entityId : cluster.entityIndices) {
                    for (const auto& e : m_allEntities) {
                        if (e.id == entityId && e.active) {
                            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, m_entitySize * 0.7f, cluster.color, 0, cluster.color);
                            ImPlot::PlotScatter("Entity", &e.x, &e.y, 1, 0, 0, sizeof(Entity));
                            break;
                        }
                    }
                }

                // Draw connections between nearby entities
                for (size_t i = 0; i < cluster.entityIndices.size(); i++) {
                    for (size_t j = i + 1; j < cluster.entityIndices.size(); j++) {
                        int entityIndex1 = cluster.entityIndices[i];
                        int entityIndex2 = cluster.entityIndices[j];
                        
                        if (entityIndex1 < static_cast<int>(m_allEntities.size()) && 
                            entityIndex2 < static_cast<int>(m_allEntities.size()) &&
                            m_allEntities[entityIndex1].active && m_allEntities[entityIndex2].active) {
                            
                            float distance = calculateDistance(
                                m_allEntities[entityIndex1].x, m_allEntities[entityIndex1].y,
                                m_allEntities[entityIndex2].x, m_allEntities[entityIndex2].y
                            );
                            if (distance <= m_dbscanEps) {
                                double x[] = {m_allEntities[entityIndex1].x, m_allEntities[entityIndex2].x};
                                double y[] = {m_allEntities[entityIndex1].y, m_allEntities[entityIndex2].y};
                                
                                ImVec4 lineColor = cluster.color;
                                lineColor.w = 0.6f;
                                ImPlot::SetNextLineStyle(lineColor, 1.0f);
                                ImPlot::PlotLine("DBSCANConnection", x, y, 2);
                            }
                        }
                    }
                }

                // Boundary overlay (Voronoi or Convex Hull)
                if (m_useVoronoi) {
                    std::vector<ImVec2> cell;
                    calculateDBSCANVoronoiCell(cluster.clusterId, cell);
                    if (cell.size() >= 3) {
                        std::vector<double> x, y;
                        for (const auto& p : cell) { x.push_back(p.x); y.push_back(p.y); }
                        x.push_back(cell[0].x); y.push_back(cell[0].y);
                        ImVec4 c = cluster.color; c.w = 0.6f;
                        ImPlot::SetNextLineStyle(c, 2.0f);
                        ImPlot::PlotLine("DBSCANVoronoiCell", x.data(), y.data(), (int)x.size());
                    }
                } else {
                    // Draw convex hull for this cluster
                    std::vector<Entity*> clusterEntities;
                    for (int entityId : cluster.entityIndices) {
                        for (auto& e : m_allEntities) {
                            if (e.id == entityId && e.active) {
                                clusterEntities.push_back(&e);
                                break;
                            }
                        }
                    }
                    
                    if (clusterEntities.size() >= 3) {
                        std::vector<ImVec2> hull;
                        calculateConvexHull(clusterEntities, hull);
                        if (hull.size() >= 3) {
                            std::vector<double> x, y;
                            for (const auto& p : hull) { x.push_back(p.x); y.push_back(p.y); }
                            x.push_back(hull[0].x); y.push_back(hull[0].y);
                            ImVec4 c = cluster.color; c.w = 0.6f;
                            ImPlot::SetNextLineStyle(c, 2.0f);
                            ImPlot::PlotLine("DBSCANConvexHull", x.data(), y.data(), (int)x.size());
                        }
                    }
                }

                ImPlot::EndPlot();
            }

            ImGui::EndChild();

            currentColumn++;
            if (currentColumn >= actualColumns) {
                ImGui::EndGroup();
                currentColumn = 0;
            } else {
                ImGui::SameLine();
            }
        }
    } else if (m_partitionVisualizationMethod == PartitionVisualizationMethod::K_MEANS) {
        // Render K-Means clusters in tiled grid
        for (const auto& cluster : m_clusters) {
            if (cluster.entityIndices.empty()) continue;

            if (currentColumn == 0) {
                ImGui::BeginGroup();
            }

            ImGui::BeginChild(("ClusterTile_" + std::to_string(cluster.clusterId)).c_str(),
                             ImVec2(tileWidth, tileHeight),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

            // Header with cluster color
            ImGui::PushStyleColor(ImGuiCol_Header, cluster.color);
            ImGui::Selectable(("C" + std::to_string(cluster.clusterId)).c_str(), false,
                             ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor();

            // Stats
            ImGui::Text("Centroid: [%.0f, %.0f]", cluster.centroidX, cluster.centroidY);
            ImGui::Text("Members: %zu", cluster.entityIndices.size());

            if (ImPlot::BeginPlot(("Cluster_" + std::to_string(cluster.clusterId) + "_Tile").c_str(),
                                 ImVec2(-1, 100), ImPlotFlags_CanvasOnly | ImPlotFlags_Equal)) {

                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);

                // Calculate cluster bounds for auto-centering and zooming
                float minX = cluster.centroidX, maxX = cluster.centroidX;
                float minY = cluster.centroidY, maxY = cluster.centroidY;
                
                for (int entityId : cluster.entityIndices) {
                    for (const auto& e : m_allEntities) {
                        if (e.id == entityId && e.active) {
                            minX = std::min(minX, e.x);
                            maxX = std::max(maxX, e.x);
                            minY = std::min(minY, e.y);
                            maxY = std::max(maxY, e.y);
                            break;
                        }
                    }
                }
                
                // Add padding and set bounds to fit the cluster
                float padding = std::max(maxX - minX, maxY - minY) * 0.2f; // 20% padding
                if (padding < 20.0f) padding = 20.0f; // Minimum padding
                
                ImPlot::SetupAxesLimits(
                    minX - padding, maxX + padding,
                    minY - padding, maxY + padding
                );

                // Draw centroid
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, m_entitySize * 1.2f, cluster.color, 2.0f, cluster.color);
                ImPlot::PlotScatter("Centroid", &cluster.centroidX, &cluster.centroidY, 1);

                // Draw entities in this cluster
                std::vector<Entity*> clusterEntities;
                clusterEntities.reserve(cluster.entityIndices.size());
                for (int entityId : cluster.entityIndices) {
                    for (auto& e : m_allEntities) {
                        if (e.id == entityId && e.active) {
                            clusterEntities.push_back(&e);
                            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, m_entitySize * 0.7f, cluster.color, 0, cluster.color);
                            ImPlot::PlotScatter("Entity", &e.x, &e.y, 1, 0, 0, sizeof(Entity));
                            break;
                        }
                    }
                }

                // Boundary overlay (Voronoi or Convex Hull)
                if (m_useVoronoi) {
                    std::vector<ImVec2> cell;
                    calculateVoronoiCell(cluster.clusterId, cell);
                    if (cell.size() >= 3) {
                        std::vector<double> x, y;
                        for (const auto& p : cell) { x.push_back(p.x); y.push_back(p.y); }
                        x.push_back(cell[0].x); y.push_back(cell[0].y);
                        ImVec4 c = cluster.color; c.w = 0.6f;
                        ImPlot::SetNextLineStyle(c, 2.0f);
                        ImPlot::PlotLine("VoronoiCell", x.data(), y.data(), (int)x.size());
                    }
                } else if (clusterEntities.size() >= 3) {
                    std::vector<ImVec2> hull;
                    calculateConvexHull(clusterEntities, hull);
                    if (hull.size() >= 3) {
                        std::vector<double> x, y;
                        for (const auto& p : hull) { x.push_back(p.x); y.push_back(p.y); }
                        x.push_back(hull[0].x); y.push_back(hull[0].y);
                        ImVec4 c = cluster.color; c.w = 0.6f;
                        ImPlot::SetNextLineStyle(c, 2.0f);
                        ImPlot::PlotLine("ConvexHull", x.data(), y.data(), (int)x.size());
                    }
                }

                ImPlot::EndPlot();
            }

            ImGui::EndChild();

            currentColumn++;
            if (currentColumn >= actualColumns) {
                ImGui::EndGroup();
                currentColumn = 0;
            } else {
                ImGui::SameLine();
            }
        }
    } else {
        // Render traditional partitions in a tiled grid
        for (const auto& partition : m_partitions) {
            if (currentColumn == 0) {
                ImGui::BeginGroup();
            }
            
            // Create tile
            ImGui::BeginChild(("PartitionTile_" + std::to_string(partition.id)).c_str(), 
                             ImVec2(tileWidth, tileHeight), 
                             ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            
            // Tile header with color indicator
            ImGui::PushStyleColor(ImGuiCol_Header, partition.color);
            ImGui::Selectable(("P" + std::to_string(partition.id)).c_str(), false, 
                             ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor();
            
            // Partition info
            ImGui::Text("Host: %s", partition.host.c_str());
            ImGui::Text("Entities: %zu", partition.entities.size());
            
            // Mini visualization
            if (ImPlot::BeginPlot(("Partition_" + std::to_string(partition.id) + "_Tile").c_str(), 
                                 ImVec2(-1, 100), ImPlotFlags_CanvasOnly | ImPlotFlags_Equal)) {
                
                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                
                // Set bounds to show entire world space for better quadtree overview
                float margin = 300.0f; // Maximum zoom out to see full quadtree
                ImPlot::SetupAxesLimits(
                    m_worldMinX - margin, m_worldMaxX + margin,
                    m_worldMinY - margin, m_worldMaxY + margin
                );
                
                // Draw only the active partitioning structure
                if (m_currentMethod == PartitioningMethod::QUADTREE) {
                    // Draw just this partition's quad bounds (not the whole tree)
                    ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 0.3f);
                    ImPlot::SetNextLineStyle(color, 1.0f);
                    double bx[] = {partition.minX, partition.maxX, partition.maxX, partition.minX, partition.minX};
                    double by[] = {partition.minY, partition.minY, partition.maxY, partition.maxY, partition.minY};
                    ImPlot::PlotLine("PartitionQuad", bx, by, 5);
                } else if (m_currentMethod == PartitioningMethod::KD_TREE) {
                    // Draw just this partition's KD-tree node bounds (not the whole tree)
                    ImVec4 color = ImVec4(0.6f, 0.8f, 0.6f, 0.3f);
                    ImPlot::SetNextLineStyle(color, 1.0f);
                    double bx[] = {partition.minX, partition.maxX, partition.maxX, partition.minX, partition.minX};
                    double by[] = {partition.minY, partition.minY, partition.maxY, partition.maxY, partition.minY};
                    ImPlot::PlotLine("PartitionKdNode", bx, by, 5);
                }
                
                // Draw entities in this partition
                for (const auto& entity : partition.entities) {
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, m_entitySize * 0.7f, entity.color, 0, entity.color);
                    ImPlot::PlotScatter("Entity", &entity.x, &entity.y, 1, 0, 0, sizeof(Entity));
                }
                
                // Only draw boundaries for clustering methods, not for basic quadtree partitions
                if (m_currentClusteringMethod != ClusteringMethod::NONE && partition.entities.size() >= 3) {
                    if (m_useVoronoi) {
                        // Draw Voronoi cell for this partition
                        std::vector<ImVec2> cell;
                        calculatePartitionVoronoiCell(partition, cell);
                        
                        if (cell.size() >= 3) {
                            std::vector<double> x, y;
                            for (const auto& point : cell) {
                                x.push_back(point.x);
                                y.push_back(point.y);
                            }
                            x.push_back(cell[0].x);
                            y.push_back(cell[0].y);
                            
                            ImVec4 cellColor = partition.color;
                            cellColor.w = 0.6f;
                            ImPlot::SetNextLineStyle(cellColor, 2.0f);
                            ImPlot::PlotLine("VoronoiCell", x.data(), y.data(), static_cast<int>(x.size()));
                        }
                    } else {
                        // Draw convex hull for this partition
                        std::vector<ImVec2> hull;
                        calculatePartitionConvexHull(partition, hull);
                        
                        if (hull.size() >= 3) {
                            std::vector<double> x, y;
                            for (const auto& point : hull) {
                                x.push_back(point.x);
                                y.push_back(point.y);
                            }
                            x.push_back(hull[0].x);
                            y.push_back(hull[0].y);
                            
                            ImVec4 hullColor = partition.color;
                            hullColor.w = 0.6f;
                            ImPlot::SetNextLineStyle(hullColor, 2.0f);
                            ImPlot::PlotLine("ConvexHull", x.data(), y.data(), static_cast<int>(x.size()));
                        }
                    }
                }
                
                ImPlot::EndPlot();
            }
            
            // Tile footer with stats
            ImGui::Separator();
            ImGui::Text("Bounds: [%.0f,%.0f] to [%.0f,%.0f]", 
                       partition.minX, partition.minY, partition.maxX, partition.maxY);
            
            ImGui::EndChild();
            
            // Handle column wrapping
            currentColumn++;
            if (currentColumn >= actualColumns) {
                ImGui::EndGroup();
                currentColumn = 0;
            } else {
                ImGui::SameLine();
            }
        }
    }
    
    // Close any remaining group
    if (currentColumn != 0) {
        ImGui::EndGroup();
    }
}


void PartitionVisualization::renderEntity(const Entity& entity) {
    ImVec4 color = entity.color;
    
    // Use cluster color if clustering is active
    if (m_currentClusteringMethod == ClusteringMethod::K_MEANS) {
        // Find which cluster this entity belongs to
        for (const auto& cluster : m_clusters) {
            for (int entityIndex : cluster.entityIndices) {
                if (entityIndex == entity.id) {
                    color = cluster.color;
                    break;
                }
            }
        }
    } else if (m_currentClusteringMethod == ClusteringMethod::DBSCAN) {
        // Find which DBSCAN cluster this entity belongs to
        for (const auto& cluster : m_dbscanClusters) {
            for (int entityIndex : cluster.entityIndices) {
                if (entityIndex == entity.id) {
                    color = cluster.color;
                    break;
                }
            }
        }
    }
    
    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, m_entitySize, color, 0, color);
    ImPlot::PlotScatter("Entity", &entity.x, &entity.y, 1, 0, 0, sizeof(Entity));
}

void PartitionVisualization::renderQuadtreeLines() {
    // Use a subtle color for the quadtree grid
    ImVec4 gridColor = ImVec4(0.8f, 0.8f, 0.8f, 0.6f);
    float thickness = 1.0f;
    drawQuadtreeNode(m_quadtree.get(), gridColor, thickness);
}

void PartitionVisualization::drawQuadtreeNode(QuadtreeNode* node, const ImVec4& color, float thickness) {
    if (!node) return;

    // Draw this node's boundary
    ImPlot::SetNextLineStyle(color, thickness);
    double x[] = {node->minX, node->maxX, node->maxX, node->minX, node->minX};
    double y[] = {node->minY, node->minY, node->maxY, node->maxY, node->minY};
    ImPlot::PlotLine("QTNode", x, y, 5);

    if (!node->isLeaf) {
        drawQuadtreeNode(node->topLeft.get(), color, thickness);
        drawQuadtreeNode(node->topRight.get(), color, thickness);
        drawQuadtreeNode(node->bottomLeft.get(), color, thickness);
        drawQuadtreeNode(node->bottomRight.get(), color, thickness);
    }
}

ImVec4 PartitionVisualization::getPartitionColor(int partitionId) {
    static std::vector<ImVec4> colors = {
        ImVec4(1.0f, 0.0f, 0.0f, 0.8f), // Red
        ImVec4(0.0f, 1.0f, 0.0f, 0.8f), // Green
        ImVec4(0.0f, 0.0f, 1.0f, 0.8f), // Blue
        ImVec4(1.0f, 1.0f, 0.0f, 0.8f), // Yellow
        ImVec4(1.0f, 0.0f, 1.0f, 0.8f), // Magenta
        ImVec4(0.0f, 1.0f, 1.0f, 0.8f), // Cyan
        ImVec4(1.0f, 0.5f, 0.0f, 0.8f), // Orange
        ImVec4(0.5f, 0.0f, 1.0f, 0.8f)  // Purple
    };
    return colors[partitionId % colors.size()];
}

ImVec4 PartitionVisualization::getEntityColor(int entityId) {
    return ImVec4(0.2f, 0.8f, 0.2f, 0.8f); // Default green
}

void PartitionVisualization::generateTestData() {
    clearData();
    addRandomEntities(16);
    generatePartitions();
}

void PartitionVisualization::generateConcentricCirclesDataset() {
    clearData();

    // Parameters
    const float centerX = 0.0f;
    const float centerY = 0.0f;
    const float halfWidth = 400.0f;
    const float halfHeight = 300.0f;
    const float margin = 20.0f;
    const float maxRadius = std::min(halfWidth, halfHeight) - margin; // ~280
    const int numRings = 4;

    std::vector<float> radii;
    radii.reserve(numRings);
    for (int i = 1; i <= numRings; ++i) {
        radii.push_back(maxRadius * (static_cast<float>(i) / numRings));
    }

    // More points on outer rings
    std::vector<int> counts;
    counts.reserve(numRings);
    for (int i = 1; i <= numRings; ++i) {
        counts.push_back(120 + 80 * i); // 200, 280, 360, 440
    }

    const float noiseSigma = 4.0f;
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::normal_distribution<float> noise(0.0f, noiseSigma);

    // Create entities on rings
    for (size_t r = 0; r < radii.size(); ++r) {
        float radius = radii[r];
        int num = counts[r];
        for (int i = 0; i < num; ++i) {
            float theta = angleDist(m_gen);
            float px = centerX + (radius + noise(m_gen)) * std::cos(theta);
            float py = centerY + (radius + noise(m_gen)) * std::sin(theta);

            Entity e{};
            e.x = px;
            e.y = py;
            e.velocityX = 0.0f;
            e.velocityY = 0.0f;
            e.id = static_cast<int>(m_allEntities.size());
            e.partitionId = -1;
            e.color = getEntityColor(e.id);
            e.active = true;
            m_allEntities.push_back(e);
        }
    }

    // Stop movement for stable clustering
    m_entitiesMoving = false;
    generatePartitions();
}

void PartitionVisualization::generateConcentricCirclesDatasetLight() {
    clearData();

    // Parameters (lighter ~500 points total)
    const float centerX = 0.0f;
    const float centerY = 0.0f;
    const float halfWidth = 400.0f;
    const float halfHeight = 300.0f;
    const float margin = 20.0f;
    const float maxRadius = std::min(halfWidth, halfHeight) - margin;
    const int numRings = 4;

    std::vector<float> radii;
    radii.reserve(numRings);
    for (int i = 1; i <= numRings; ++i) {
        radii.push_back(maxRadius * (static_cast<float>(i) / numRings));
    }

    std::vector<int> counts = {70, 110, 150, 180}; // ~510
    const float noiseSigma = 4.0f;
    std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
    std::normal_distribution<float> noise(0.0f, noiseSigma);

    for (size_t r = 0; r < radii.size(); ++r) {
        float radius = radii[r];
        int num = counts[r];
        for (int i = 0; i < num; ++i) {
            float theta = angleDist(m_gen);
            float px = centerX + (radius + noise(m_gen)) * std::cos(theta);
            float py = centerY + (radius + noise(m_gen)) * std::sin(theta);

            Entity e{};
            e.x = px;
            e.y = py;
            e.velocityX = 0.0f;
            e.velocityY = 0.0f;
            e.id = static_cast<int>(m_allEntities.size());
            e.partitionId = -1;
            e.color = getEntityColor(e.id);
            e.active = true;
            m_allEntities.push_back(e);
        }
    }

    m_entitiesMoving = false;
    generatePartitions();
}

void PartitionVisualization::clearData() {
    m_partitions.clear();
    m_allEntities.clear();
    m_clusters.clear();
    m_dbscanClusters.clear();
    m_dbscanEntityLabels.clear();
    clearQuadtree();
    clearKdTree();
}

void PartitionVisualization::addRandomEntities(int count) {
    std::uniform_real_distribution<float> velDist(-100.0f, 100.0f);
    
    for (int i = 0; i < count; i++) {
        Entity entity;
        entity.x = m_posDist(m_gen);
        entity.y = m_posDist(m_gen);
        entity.velocityX = velDist(m_gen);
        entity.velocityY = velDist(m_gen);
        entity.id = static_cast<int>(m_allEntities.size());
        entity.partitionId = -1; // Unassigned
        entity.color = getEntityColor(entity.id);
        entity.active = true;
        
        m_allEntities.push_back(entity);
    }
}

void PartitionVisualization::addEntityAt(float x, float y) {
    x = std::max(m_worldMinX, std::min(m_worldMaxX, x));
    y = std::max(m_worldMinY, std::min(m_worldMaxY, y));

    Entity entity;
    entity.x = x;
    entity.y = y;
    
    // Give random velocity if movement is enabled
    if (m_entitiesMoving) {
        std::uniform_real_distribution<float> velDist(-m_entitySpeed, m_entitySpeed);
        entity.velocityX = velDist(m_gen);
        entity.velocityY = velDist(m_gen);
    } else {
        entity.velocityX = 0.0f;
        entity.velocityY = 0.0f;
    }
    
    entity.id = static_cast<int>(m_allEntities.size());
    entity.partitionId = -1;
    entity.color = getEntityColor(entity.id);
    entity.active = true;

    m_allEntities.push_back(entity);
}

void PartitionVisualization::handleEntityClick(float clickX, float clickY) {
    if (!m_entitiesMoving) return;
    
    const float clickRadius = 15.0f; // Click detection radius
    std::uniform_real_distribution<float> velDist(-m_entitySpeed, m_entitySpeed);
    
    // Find the closest entity within click radius
    Entity* closestEntity = nullptr;
    float closestDistance = std::numeric_limits<float>::max();
    
    for (auto& entity : m_allEntities) {
        if (!entity.active) continue;
        
        float distance = calculateDistance(clickX, clickY, entity.x, entity.y);
        if (distance <= clickRadius && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = &entity;
        }
    }
    
    // Give the clicked entity random velocity
    if (closestEntity) {
        closestEntity->velocityX = velDist(m_gen);
        closestEntity->velocityY = velDist(m_gen);
    }
}



// Quadtree functionality implementation
void PartitionVisualization::generateQuadtreePartitions() {
    // Clear existing partitions
    m_partitions.clear();
    
    // Build quadtree
    buildQuadtree();
    
    // Collect partitions from quadtree (only those with entities)
    collectQuadtreePartitions(m_quadtree.get());
}

void PartitionVisualization::buildQuadtree() {
    // Clear existing quadtree
    clearQuadtree();
    
    // Create root node
    m_quadtree = std::make_unique<QuadtreeNode>(m_worldMinX, m_worldMinY, m_worldMaxX, m_worldMaxY);
    
    // Insert all entities into quadtree
    for (auto& entity : m_allEntities) {
        insertEntityIntoQuadtree(&entity, m_quadtree.get());
    }
}

void PartitionVisualization::insertEntityIntoQuadtree(Entity* entity, QuadtreeNode* node) {
    // Check if entity is within this node's bounds
    if (entity->x < node->minX || entity->x > node->maxX || 
        entity->y < node->minY || entity->y > node->maxY) {
        return;
    }
    
    if (node->isLeaf) {
        // Add entity to this leaf node
        node->entities.push_back(entity);
        
        // Check if we need to subdivide
        if (node->entities.size() > node->maxEntities && node->depth < node->maxDepth) {
            subdivideNode(node);
        }
    } else {
        // Insert into appropriate child
        float midX = (node->minX + node->maxX) * 0.5f;
        float midY = (node->minY + node->maxY) * 0.5f;
        
        if (entity->x < midX && entity->y < midY) {
            insertEntityIntoQuadtree(entity, node->bottomLeft.get());
        } else if (entity->x >= midX && entity->y < midY) {
            insertEntityIntoQuadtree(entity, node->bottomRight.get());
        } else if (entity->x < midX && entity->y >= midY) {
            insertEntityIntoQuadtree(entity, node->topLeft.get());
        } else {
            insertEntityIntoQuadtree(entity, node->topRight.get());
        }
    }
}

void PartitionVisualization::subdivideNode(QuadtreeNode* node) {
    if (!node->isLeaf) return;
    
    float midX = (node->minX + node->maxX) * 0.5f;
    float midY = (node->minY + node->maxY) * 0.5f;
    
    // Create child nodes
    node->topLeft = std::make_unique<QuadtreeNode>(node->minX, midY, midX, node->maxY, node->depth + 1, node->maxDepth, node->maxEntities);
    node->topRight = std::make_unique<QuadtreeNode>(midX, midY, node->maxX, node->maxY, node->depth + 1, node->maxDepth, node->maxEntities);
    node->bottomLeft = std::make_unique<QuadtreeNode>(node->minX, node->minY, midX, midY, node->depth + 1, node->maxDepth, node->maxEntities);
    node->bottomRight = std::make_unique<QuadtreeNode>(midX, node->minY, node->maxX, midY, node->depth + 1, node->maxDepth, node->maxEntities);
    
    // Redistribute entities to children
    std::vector<Entity*> entitiesToRedistribute = node->entities;
    node->entities.clear();
    node->isLeaf = false;
    
    for (Entity* entity : entitiesToRedistribute) {
        insertEntityIntoQuadtree(entity, node);
    }
}

void PartitionVisualization::collectQuadtreePartitions(QuadtreeNode* node) {
    if (!node) return;
    
    // Only create partitions for nodes that have entities
    if (!node->entities.empty()) {
        Partition partition;
        partition.id = static_cast<int>(m_partitions.size());
        partition.host = "Machine" + std::to_string((partition.id % 2) + 1); // Alternate between Machine1 and Machine2
        partition.color = getPartitionColor(partition.id);
        partition.minX = node->minX;
        partition.minY = node->minY;
        partition.maxX = node->maxX;
        partition.maxY = node->maxY;
        
        // Add entities to this partition
        for (Entity* entity : node->entities) {
            partition.entities.push_back(*entity);
        }
        
        m_partitions.push_back(partition);
    }
    
    // Recursively process children
    if (!node->isLeaf) {
        collectQuadtreePartitions(node->topLeft.get());
        collectQuadtreePartitions(node->topRight.get());
        collectQuadtreePartitions(node->bottomLeft.get());
        collectQuadtreePartitions(node->bottomRight.get());
    }
}

void PartitionVisualization::clearQuadtree() {
    m_quadtree.reset();
}

// KD-tree functionality implementation
void PartitionVisualization::buildKdTree() {
    // Clear existing KD-tree
    clearKdTree();
    
    // Create root node
    m_kdtree = std::make_unique<KdTreeNode>(m_worldMinX, m_worldMinY, m_worldMaxX, m_worldMaxY);
    
    // Insert all entities into KD-tree
    for (auto& entity : m_allEntities) {
        insertEntityIntoKdTree(&entity, m_kdtree.get());
    }
}

void PartitionVisualization::insertEntityIntoKdTree(Entity* entity, KdTreeNode* node) {
    // Check if entity is within this node's bounds
    if (entity->x < node->minX || entity->x > node->maxX || 
        entity->y < node->minY || entity->y > node->maxY) {
        return;
    }
    
    if (node->isLeaf) {
        // Add entity to this leaf node
        node->entities.push_back(entity);
        
        // Check if we need to subdivide
        if (node->entities.size() > node->maxEntities && node->depth < node->maxDepth) {
            subdivideKdTreeNode(node);
        }
    } else {
        // Insert into appropriate child based on split axis and value
        if (node->splitAxis == 0) { // X-axis split
            if (entity->x < node->splitValue) {
                insertEntityIntoKdTree(entity, node->left.get());
            } else {
                insertEntityIntoKdTree(entity, node->right.get());
            }
        } else { // Y-axis split
            if (entity->y < node->splitValue) {
                insertEntityIntoKdTree(entity, node->left.get());
            } else {
                insertEntityIntoKdTree(entity, node->right.get());
            }
        }
    }
}

void PartitionVisualization::subdivideKdTreeNode(KdTreeNode* node) {
    if (!node->isLeaf) return;
    
    // Determine split axis (alternate between X and Y)
    node->splitAxis = node->depth % 2; // 0 for X-axis, 1 for Y-axis
    
    // Calculate median value for splitting
    std::vector<float> values;
    for (Entity* entity : node->entities) {
        if (node->splitAxis == 0) {
            values.push_back(entity->x);
        } else {
            values.push_back(entity->y);
        }
    }
    
    // Sort values to find median
    std::sort(values.begin(), values.end());
    node->splitValue = values[values.size() / 2];
    
    // Create child nodes
    if (node->splitAxis == 0) { // X-axis split
        node->left = std::make_unique<KdTreeNode>(node->minX, node->minY, node->splitValue, node->maxY, 
                                                node->depth + 1, node->maxDepth, node->maxEntities);
        node->right = std::make_unique<KdTreeNode>(node->splitValue, node->minY, node->maxX, node->maxY, 
                                                 node->depth + 1, node->maxDepth, node->maxEntities);
    } else { // Y-axis split
        node->left = std::make_unique<KdTreeNode>(node->minX, node->minY, node->maxX, node->splitValue, 
                                                node->depth + 1, node->maxDepth, node->maxEntities);
        node->right = std::make_unique<KdTreeNode>(node->minX, node->splitValue, node->maxX, node->maxY, 
                                                 node->depth + 1, node->maxDepth, node->maxEntities);
    }
    
    // Redistribute entities to children
    std::vector<Entity*> entitiesToRedistribute = node->entities;
    node->entities.clear();
    node->isLeaf = false;
    
    for (Entity* entity : entitiesToRedistribute) {
        insertEntityIntoKdTree(entity, node);
    }
}

void PartitionVisualization::collectKdTreePartitions(KdTreeNode* node) {
    if (!node) return;
    
    // Only create partitions for nodes that have entities
    if (!node->entities.empty()) {
        Partition partition;
        partition.id = static_cast<int>(m_partitions.size());
        partition.host = "Machine" + std::to_string((partition.id % 2) + 1); // Alternate between Machine1 and Machine2
        partition.color = getPartitionColor(partition.id);
        partition.minX = node->minX;
        partition.minY = node->minY;
        partition.maxX = node->maxX;
        partition.maxY = node->maxY;
        
        // Add entities to this partition
        for (Entity* entity : node->entities) {
            partition.entities.push_back(*entity);
        }
        
        m_partitions.push_back(partition);
    }
    
    // Recursively process children
    if (!node->isLeaf) {
        collectKdTreePartitions(node->left.get());
        collectKdTreePartitions(node->right.get());
    }
}

void PartitionVisualization::clearKdTree() {
    m_kdtree.reset();
}

void PartitionVisualization::renderKdTreeLines() {
    // Use a subtle color for the KD-tree grid
    ImVec4 gridColor = ImVec4(0.6f, 0.8f, 0.6f, 0.6f);
    float thickness = 1.0f;
    drawKdTreeNode(m_kdtree.get(), gridColor, thickness);
}

void PartitionVisualization::drawKdTreeNode(KdTreeNode* node, const ImVec4& color, float thickness) {
    if (!node) return;

    // Draw this node's boundary
    ImPlot::SetNextLineStyle(color, thickness);
    double x[] = {node->minX, node->maxX, node->maxX, node->minX, node->minX};
    double y[] = {node->minY, node->minY, node->maxY, node->maxY, node->minY};
    ImPlot::PlotLine("KdTreeNode", x, y, 5);

    if (!node->isLeaf) {
        // Draw the split line
        ImVec4 splitColor = ImVec4(1.0f, 0.5f, 0.0f, 0.8f); // Orange for split lines
        ImPlot::SetNextLineStyle(splitColor, thickness + 0.5f);
        
        if (node->splitAxis == 0) { // X-axis split (vertical line)
            double splitX[] = {node->splitValue, node->splitValue};
            double splitY[] = {node->minY, node->maxY};
            ImPlot::PlotLine("KdTreeSplit", splitX, splitY, 2);
        } else { // Y-axis split (horizontal line)
            double splitX[] = {node->minX, node->maxX};
            double splitY[] = {node->splitValue, node->splitValue};
            ImPlot::PlotLine("KdTreeSplit", splitX, splitY, 2);
        }
        
        // Recursively draw children
        drawKdTreeNode(node->left.get(), color, thickness);
        drawKdTreeNode(node->right.get(), color, thickness);
    }
}

// K-means clustering implementation

void PartitionVisualization::performKMeansClustering() {
    if (m_allEntities.empty()) return;
    
    // Clear existing clusters
    m_clusters.clear();
    
    // Initialize centroids using quadtree if available, otherwise random
    if (m_quadtree) {
        initializeKMeansCentroidsFromQuadtree();
    } else {
        initializeKMeansCentroids();
    }
    
    // Perform K-means iterations
    for (int iteration = 0; iteration < m_kMeansMaxIterations; iteration++) {
        // Store previous centroids for convergence check
        std::vector<std::pair<float, float>> previousCentroids;
        for (const auto& cluster : m_clusters) {
            previousCentroids.push_back({cluster.centroidX, cluster.centroidY});
        }
        
        // Assign entities to clusters using quadtree if available
        if (m_quadtree) {
            assignEntitiesToClustersUsingQuadtree();
        } else {
            assignEntitiesToClusters();
        }
        
        // Update cluster centroids
        if (m_quadtree) {
            updateClusterCentroidsFromQuadtree();
        } else {
            updateClusterCentroids();
        }
        
        // Check for convergence
        if (hasKMeansConverged()) {
            break;
        }
    }
    
    // Update entity colors based on cluster assignments
    updateEntityColorsFromClusters();
}

void PartitionVisualization::performDBSCANClustering() {
    if (m_allEntities.empty()) return;
    
    // Clear previous results
    m_dbscanClusters.clear();
    resetDBSCANLabels();
    
    int nextClusterId = 0;
    
    // Process each entity
    for (int i = 0; i < static_cast<int>(m_allEntities.size()); i++) {
        if (!m_allEntities[i].active) continue;
        if (m_dbscanEntityLabels[i] != DBSCAN_UNVISITED) continue; // already visited

        // Get neighbors including the point itself
        std::vector<int> neighbors = getDBSCANNeighbors(i);

        // Core point check: |N_eps(p)| >= MinPts
        if (static_cast<int>(neighbors.size()) < m_dbscanMinPts) {
            m_dbscanEntityLabels[i] = DBSCAN_NOISE; // mark as noise for now (may be upgraded to border)
            continue;
        }

        // Start new cluster
        int clusterId = nextClusterId++;
        DBSCANCluster cluster;
        cluster.clusterId = clusterId;
        cluster.color = getDBSCANClusterColor(clusterId);
        cluster.entityIndices.clear();

        // Expand cluster
        expandDBSCANCluster(i, clusterId);

        // Collect members for visualization
        for (int j = 0; j < static_cast<int>(m_allEntities.size()); j++) {
            if (m_allEntities[j].active && m_dbscanEntityLabels[j] == clusterId) {
                cluster.entityIndices.push_back(j);
            }
        }

        if (!cluster.entityIndices.empty()) {
            m_dbscanClusters.push_back(cluster);
        }
    }
    
    // Update entity colors based on cluster assignments
    updateDBSCANEntityColors();
}

void PartitionVisualization::clearClustering() {
    m_clusters.clear();
    m_dbscanClusters.clear();
    m_dbscanEntityLabels.clear();
    
    // Reset entity colors to default
    for (auto& entity : m_allEntities) {
        entity.color = getEntityColor(entity.id);
    }
}

void PartitionVisualization::initializeKMeansCentroids() {
    m_clusters.clear();
    m_clusters.resize(m_kMeansK);
    
    // Get bounds of all entities
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    
    for (const auto& entity : m_allEntities) {
        minX = std::min(minX, entity.x);
        maxX = std::max(maxX, entity.x);
        minY = std::min(minY, entity.y);
        maxY = std::max(maxY, entity.y);
    }
    
    // Initialize centroids randomly within bounds
    std::uniform_real_distribution<float> xDist(minX, maxX);
    std::uniform_real_distribution<float> yDist(minY, maxY);
    
    for (int i = 0; i < m_kMeansK; i++) {
        m_clusters[i].clusterId = i;
        m_clusters[i].centroidX = xDist(m_gen);
        m_clusters[i].centroidY = yDist(m_gen);
        m_clusters[i].color = getClusterColor(i);
        m_clusters[i].entityIndices.clear();
    }
}

void PartitionVisualization::assignEntitiesToClusters() {
    // Clear entity assignments
    for (auto& cluster : m_clusters) {
        cluster.entityIndices.clear();
    }
    
    // Assign each entity to the nearest centroid
    for (const auto& entity : m_allEntities) {
        if (!entity.active) continue;
        
        int bestCluster = 0;
        float bestDistance = calculateDistance(entity.x, entity.y, m_clusters[0].centroidX, m_clusters[0].centroidY);
        
        for (int i = 1; i < m_clusters.size(); i++) {
            float distance = calculateDistance(entity.x, entity.y, m_clusters[i].centroidX, m_clusters[i].centroidY);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestCluster = i;
            }
        }
        
        m_clusters[bestCluster].entityIndices.push_back(entity.id);
    }
}

void PartitionVisualization::updateClusterCentroids() {
    for (auto& cluster : m_clusters) {
        if (cluster.entityIndices.empty()) continue;
        
        float sumX = 0.0f, sumY = 0.0f;
        int count = 0;
        
        for (int entityId : cluster.entityIndices) {
            // Find entity by ID
            for (const auto& entity : m_allEntities) {
                if (entity.id == entityId && entity.active) {
                    sumX += entity.x;
                    sumY += entity.y;
                    count++;
                    break;
                }
            }
        }
        
        if (count > 0) {
            cluster.centroidX = sumX / count;
            cluster.centroidY = sumY / count;
        }
    }
}

void PartitionVisualization::updateEntityColorsFromClusters() {
    for (auto& entity : m_allEntities) {
        if (!entity.active) continue;
        
        // Find which cluster this entity belongs to
        for (const auto& cluster : m_clusters) {
            for (int entityId : cluster.entityIndices) {
                if (entityId == entity.id) {
                    entity.color = cluster.color;
                    break;
                }
            }
        }
    }
}

ImVec4 PartitionVisualization::getClusterColor(int clusterIndex) {
    static std::vector<ImVec4> colors = {
        ImVec4(1.0f, 0.0f, 0.0f, 0.8f), // Red
        ImVec4(0.0f, 1.0f, 0.0f, 0.8f), // Green
        ImVec4(0.0f, 0.0f, 1.0f, 0.8f), // Blue
        ImVec4(1.0f, 1.0f, 0.0f, 0.8f), // Yellow
        ImVec4(1.0f, 0.0f, 1.0f, 0.8f), // Magenta
        ImVec4(0.0f, 1.0f, 1.0f, 0.8f), // Cyan
        ImVec4(1.0f, 0.5f, 0.0f, 0.8f), // Orange
        ImVec4(0.5f, 0.0f, 1.0f, 0.8f)  // Purple
    };
    return colors[clusterIndex % colors.size()];
}

float PartitionVisualization::calculateDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

bool PartitionVisualization::hasKMeansConverged() {
    // Simple convergence check - could be improved with proper centroid tracking
    return false; // For now, let it run for max iterations
}

void PartitionVisualization::renderClusterCentroids() {
    for (const auto& cluster : m_clusters) {
        if (cluster.entityIndices.empty()) continue;
        
        // Draw centroid as a larger, distinct marker
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, m_entitySize * 1.5f, cluster.color, 2.0f, cluster.color);
        ImPlot::PlotScatter("Centroid", &cluster.centroidX, &cluster.centroidY, 1, 0, 0, sizeof(float));
    }
}

void PartitionVisualization::renderClusterLines() {
    for (const auto& cluster : m_clusters) {
        if (cluster.entityIndices.empty()) continue;
        
        // Draw lines from centroid to each entity in this cluster
        for (int entityId : cluster.entityIndices) {
            // Find the entity by ID
            for (const auto& entity : m_allEntities) {
                if (entity.id == entityId && entity.active) {
                    // Create line data from centroid to entity
                    double x[] = {cluster.centroidX, entity.x};
                    double y[] = {cluster.centroidY, entity.y};
                    
                    // Use cluster color with some transparency for the lines
                    ImVec4 lineColor = cluster.color;
                    lineColor.w = 0.6f; // Make lines semi-transparent
                    
                    ImPlot::SetNextLineStyle(lineColor, 1.5f);
                    ImPlot::PlotLine("ClusterLine", x, y, 2);
                    break;
                }
            }
        }
    }
}

void PartitionVisualization::renderConvexHulls() {
    for (const auto& cluster : m_clusters) {
        if (cluster.entityIndices.empty()) continue;
        
        // Collect entities for this cluster
        std::vector<Entity*> clusterEntities;
        for (int entityId : cluster.entityIndices) {
            for (auto& entity : m_allEntities) {
                if (entity.id == entityId && entity.active) {
                    clusterEntities.push_back(&entity);
                    break;
                }
            }
        }
        
        if (clusterEntities.size() < 3) continue; // Need at least 3 points for a hull
        
        // Calculate convex hull
        std::vector<ImVec2> hull;
        calculateConvexHull(clusterEntities, hull);
        
        if (hull.size() < 3) continue;
        
        // Convert to arrays for plotting
        std::vector<double> x, y;
        for (const auto& point : hull) {
            x.push_back(point.x);
            y.push_back(point.y);
        }
        // Close the hull
        x.push_back(hull[0].x);
        y.push_back(hull[0].y);
        
        // Draw convex hull with cluster color
        ImVec4 hullColor = cluster.color;
        hullColor.w = 0.3f; // Make hull semi-transparent
        
        ImPlot::SetNextLineStyle(hullColor, 2.0f);
        ImPlot::PlotLine("ConvexHull", x.data(), y.data(), static_cast<int>(x.size()));
    }
}

void PartitionVisualization::calculateConvexHull(const std::vector<Entity*>& entities, std::vector<ImVec2>& hull) {
    if (entities.size() < 3) return;
    
    hull.clear();
    
    // Find the bottom-most point (and leftmost in case of tie)
    int bottomIndex = 0;
    for (size_t i = 1; i < entities.size(); i++) {
        if (entities[i]->y < entities[bottomIndex]->y || 
            (entities[i]->y == entities[bottomIndex]->y && entities[i]->x < entities[bottomIndex]->x)) {
            bottomIndex = static_cast<int>(i);
        }
    }
    
    // Sort points by polar angle with respect to bottom point
    std::vector<std::pair<float, int>> angles;
    for (size_t i = 0; i < entities.size(); i++) {
        if (i == bottomIndex) continue;
        
        float dx = entities[i]->x - entities[bottomIndex]->x;
        float dy = entities[i]->y - entities[bottomIndex]->y;
        float angle = std::atan2(dy, dx);
        angles.push_back({angle, static_cast<int>(i)});
    }
    
    std::sort(angles.begin(), angles.end());
    
    // Build convex hull using Graham scan
    hull.push_back({entities[bottomIndex]->x, entities[bottomIndex]->y});
    
    for (const auto& anglePair : angles) {
        int pointIndex = anglePair.second;
        hull.push_back({entities[pointIndex]->x, entities[pointIndex]->y});
        
        // Remove points that make a non-left turn
        while (hull.size() > 2) {
            ImVec2 p1 = hull[hull.size() - 3];
            ImVec2 p2 = hull[hull.size() - 2];
            ImVec2 p3 = hull[hull.size() - 1];
            
            // Calculate cross product to determine turn direction
            float cross = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
            if (cross <= 0) {
                // Remove middle point (non-left turn)
                hull.erase(hull.end() - 2);
            } else {
                break;
            }
        }
    }
}

// Quadtree-based K-means implementation
void PartitionVisualization::updateKMeansIfNeeded(float dt) {
    m_kMeansUpdateTimer += dt;
    
    if (m_kMeansUpdateTimer >= m_kMeansUpdateInterval) {
        // Perform a quick K-means update using quadtree
        if (m_quadtree && !m_clusters.empty()) {
            // Quick assignment using quadtree
            assignEntitiesToClustersUsingQuadtree();
            updateClusterCentroidsFromQuadtree();
            updateEntityColorsFromClusters();
        }
        m_kMeansUpdateTimer = 0.0f;
    }
}

void PartitionVisualization::initializeKMeansCentroidsFromQuadtree() {
    m_clusters.clear();
    m_clusters.resize(m_kMeansK);
    
    // Collect all entities from quadtree
    std::vector<Entity*> allEntities;
    collectAllEntitiesFromQuadtree(m_quadtree.get(), allEntities);
    
    if (allEntities.empty()) return;
    
    // Use K-means++ initialization for better centroids
    // Start with a random entity
    std::uniform_int_distribution<int> entityDist(0, static_cast<int>(allEntities.size()) - 1);
    int firstIndex = entityDist(m_gen);
    m_clusters[0].centroidX = allEntities[firstIndex]->x;
    m_clusters[0].centroidY = allEntities[firstIndex]->y;
    m_clusters[0].clusterId = 0;
    m_clusters[0].color = getClusterColor(0);
    m_clusters[0].entityIndices.clear();
    
    // Select remaining centroids using K-means++ strategy
    for (int k = 1; k < m_kMeansK; k++) {
        std::vector<float> distances;
        float totalDistance = 0.0f;
        
        // Calculate minimum distance to existing centroids for each entity
        for (Entity* entity : allEntities) {
            float minDist = std::numeric_limits<float>::max();
            for (int i = 0; i < k; i++) {
                float dist = calculateDistance(entity->x, entity->y, m_clusters[i].centroidX, m_clusters[i].centroidY);
                minDist = std::min(minDist, dist);
            }
            distances.push_back(minDist * minDist); // Square distance for probability
            totalDistance += distances.back();
        }
        
        // Select next centroid with probability proportional to squared distance
        std::uniform_real_distribution<float> probDist(0.0f, totalDistance);
        float randomValue = probDist(m_gen);
        float cumulativeDistance = 0.0f;
        
        for (size_t i = 0; i < allEntities.size(); i++) {
            cumulativeDistance += distances[i];
            if (cumulativeDistance >= randomValue) {
                m_clusters[k].centroidX = allEntities[i]->x;
                m_clusters[k].centroidY = allEntities[i]->y;
                m_clusters[k].clusterId = k;
                m_clusters[k].color = getClusterColor(k);
                m_clusters[k].entityIndices.clear();
                break;
            }
        }
    }
}

void PartitionVisualization::assignEntitiesToClustersUsingQuadtree() {
    // Clear entity assignments
    for (auto& cluster : m_clusters) {
        cluster.entityIndices.clear();
    }
    
    // Use quadtree to efficiently find nearest centroids
    for (auto& entity : m_allEntities) {
        if (!entity.active) continue;
        
        int bestCluster = 0;
        float bestDistance = calculateDistance(entity.x, entity.y, m_clusters[0].centroidX, m_clusters[0].centroidY);
        
        // Find nearest centroid using quadtree spatial queries
        for (int i = 1; i < m_clusters.size(); i++) {
            float distance = calculateDistance(entity.x, entity.y, m_clusters[i].centroidX, m_clusters[i].centroidY);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestCluster = i;
            }
        }
        
        m_clusters[bestCluster].entityIndices.push_back(entity.id);
    }
}

void PartitionVisualization::updateClusterCentroidsFromQuadtree() {
    for (auto& cluster : m_clusters) {
        if (cluster.entityIndices.empty()) continue;
        
        float sumX = 0.0f, sumY = 0.0f;
        int count = 0;
        
        // Use quadtree to efficiently find entities
        for (int entityId : cluster.entityIndices) {
            // Find entity by ID in quadtree
            Entity* entity = findEntityInQuadtree(m_quadtree.get(), entityId);
            if (entity && entity->active) {
                sumX += entity->x;
                sumY += entity->y;
                count++;
            }
        }
        
        if (count > 0) {
            cluster.centroidX = sumX / count;
            cluster.centroidY = sumY / count;
        }
    }
}

// (removed unused findEntitiesInQuadtreeRegion)

// Helper methods for quadtree traversal
void PartitionVisualization::collectAllEntitiesFromQuadtree(QuadtreeNode* node, std::vector<Entity*>& entities) {
    if (!node) return;
    
    // Add entities from this node
    for (Entity* entity : node->entities) {
        if (entity->active) {
            entities.push_back(entity);
        }
    }
    
    // Recursively collect from children
    if (!node->isLeaf) {
        collectAllEntitiesFromQuadtree(node->topLeft.get(), entities);
        collectAllEntitiesFromQuadtree(node->topRight.get(), entities);
        collectAllEntitiesFromQuadtree(node->bottomLeft.get(), entities);
        collectAllEntitiesFromQuadtree(node->bottomRight.get(), entities);
    }
}

Entity* PartitionVisualization::findEntityInQuadtree(QuadtreeNode* node, int entityId) {
    if (!node) return nullptr;
    
    // Check entities in this node
    for (Entity* entity : node->entities) {
        if (entity->id == entityId) {
            return entity;
        }
    }
    
    // Recursively search children
    if (!node->isLeaf) {
        Entity* found = findEntityInQuadtree(node->topLeft.get(), entityId);
        if (found) return found;
        
        found = findEntityInQuadtree(node->topRight.get(), entityId);
        if (found) return found;
        
        found = findEntityInQuadtree(node->bottomLeft.get(), entityId);
        if (found) return found;
        
        found = findEntityInQuadtree(node->bottomRight.get(), entityId);
        if (found) return found;
    }
    
    return nullptr;
}

// Voronoi diagram implementation
void PartitionVisualization::renderVoronoiCells() {
    if (m_clusters.empty()) return;
    
    // Collect all cluster centroids as Voronoi sites
    std::vector<ImVec2> allSites;
    for (const auto& cluster : m_clusters) {
        allSites.push_back({cluster.centroidX, cluster.centroidY});
    }
    
    // Calculate bounds for Voronoi diagram
    ImVec2 boundsCenter = {(m_worldMinX + m_worldMaxX) * 0.5f, (m_worldMinY + m_worldMaxY) * 0.5f};
    ImVec2 boundsSize = {m_worldMaxX - m_worldMinX, m_worldMaxY - m_worldMinY};
    
    // Render Voronoi cell for each cluster
    for (size_t i = 0; i < m_clusters.size(); i++) {
        const auto& cluster = m_clusters[i];
        ImVec2 site = {cluster.centroidX, cluster.centroidY};
        
        auto cell = computeVoronoiCell(site, allSites, boundsCenter, boundsSize);
        
        if (cell.size() >= 3) {
            std::vector<double> x, y;
            for (const auto& point : cell) {
                x.push_back(point.x);
                y.push_back(point.y);
            }
            x.push_back(cell[0].x);
            y.push_back(cell[0].y);
            
            ImVec4 cellColor = cluster.color;
            cellColor.w = 0.3f; // Semi-transparent
            ImPlot::SetNextLineStyle(cellColor, 2.0f);
            ImPlot::PlotLine("VoronoiCell", x.data(), y.data(), static_cast<int>(x.size()));
        }
    }
}

void PartitionVisualization::calculateVoronoiCell(int clusterIndex, std::vector<ImVec2>& cell) {
    if (clusterIndex >= m_clusters.size()) return;
    
    const auto& cluster = m_clusters[clusterIndex];
    ImVec2 site = {cluster.centroidX, cluster.centroidY};
    
    // Collect all cluster centroids as Voronoi sites
    std::vector<ImVec2> allSites;
    for (const auto& c : m_clusters) {
        allSites.push_back({c.centroidX, c.centroidY});
    }
    
    // Use world bounds for Voronoi cell calculation in tiles
    ImVec2 boundsCenter = {(m_worldMinX + m_worldMaxX) * 0.5f, (m_worldMinY + m_worldMaxY) * 0.5f};
    ImVec2 boundsSize = {m_worldMaxX - m_worldMinX, m_worldMaxY - m_worldMinY};
    
    cell = computeVoronoiCell(site, allSites, boundsCenter, boundsSize);
}

std::vector<ImVec2> PartitionVisualization::clipPolygonWithHalfPlane(const std::vector<ImVec2>& poly, const HalfPlane& hp) {
    std::vector<ImVec2> out;
    if (poly.empty()) return out;
    
    auto inside = [&](const ImVec2& p) -> bool { 
        return hp.n.x * p.x + hp.n.y * p.y <= hp.d + 1e-4f; 
    };
    
    auto intersect = [&](const ImVec2& a, const ImVec2& b) -> ImVec2 {
        // Solve for t where n·(a + t(b-a)) = d
        ImVec2 ab = {b.x - a.x, b.y - a.y};
        float denom = hp.n.x * ab.x + hp.n.y * ab.y;
        if (std::abs(denom) < 1e-6f) return a; // parallel, return a
        float t = (hp.d - (hp.n.x * a.x + hp.n.y * a.y)) / denom;
        return {a.x + ab.x * t, a.y + ab.y * t};
    };

    for (size_t i = 0; i < poly.size(); ++i) {
        ImVec2 curr = poly[i];
        ImVec2 prev = poly[(i + poly.size() - 1) % poly.size()];
        bool currIn = inside(curr);
        bool prevIn = inside(prev);
        if (currIn) {
            if (!prevIn) out.push_back(intersect(prev, curr));
            out.push_back(curr);
        } else if (prevIn) {
            out.push_back(intersect(prev, curr));
        }
    }
    return out;
}

std::vector<ImVec2> PartitionVisualization::computeVoronoiCell(const ImVec2& site, const std::vector<ImVec2>& allSites, const ImVec2& boundsCenter, const ImVec2& boundsSize) {
    // Start with rectangle bounds polygon
    ImVec2 hs = {boundsSize.x * 0.5f, boundsSize.y * 0.5f};
    std::vector<ImVec2> poly = {
        {boundsCenter.x - hs.x, boundsCenter.y - hs.y},
        {boundsCenter.x + hs.x, boundsCenter.y - hs.y},
        {boundsCenter.x + hs.x, boundsCenter.y + hs.y},
        {boundsCenter.x - hs.x, boundsCenter.y + hs.y}
    };

    for (const auto& other : allSites) {
        if (other.x == site.x && other.y == site.y) continue;
        // Bisector half-plane: keep points closer to 'site' than 'other'
        ImVec2 m = {(site.x + other.x) * 0.5f, (site.y + other.y) * 0.5f}; // midpoint
        ImVec2 n = {other.x - site.x, other.y - site.y}; // normal pointing from site to other
        // Half-plane equation: n·x <= n·m
        HalfPlane hp{ n, n.x * m.x + n.y * m.y };
        poly = clipPolygonWithHalfPlane(poly, hp);
        if (poly.empty()) break;
    }

    return poly;
}

// DBSCAN implementation
void PartitionVisualization::expandDBSCANCluster(int entityIndex, int clusterId) {
    // BFS over density-reachable set
    std::vector<int> queue = getDBSCANNeighbors(entityIndex); // includes the point itself
    // Assign the seed point
    m_dbscanEntityLabels[entityIndex] = clusterId;

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        int current = queue[qi];

        // If it was noise, upgrade to border (cluster member)
        if (m_dbscanEntityLabels[current] == DBSCAN_NOISE) {
            m_dbscanEntityLabels[current] = clusterId;
        }

        // If unvisited, assign to cluster
        if (m_dbscanEntityLabels[current] == DBSCAN_UNVISITED) {
            m_dbscanEntityLabels[current] = clusterId;

            // Check if current is a core point
            std::vector<int> currentNeighbors = getDBSCANNeighbors(current);
            if (static_cast<int>(currentNeighbors.size()) >= m_dbscanMinPts) {
                // Append neighbors to queue for further expansion
                for (int nb : currentNeighbors) {
                    if (std::find(queue.begin(), queue.end(), nb) == queue.end()) {
                        queue.push_back(nb);
                    }
                }
            }
        }
    }
}

std::vector<int> PartitionVisualization::getDBSCANNeighbors(int entityIndex) {
    std::vector<int> neighbors;
    
    if (entityIndex < 0 || entityIndex >= static_cast<int>(m_allEntities.size())) return neighbors;
    if (!m_allEntities[entityIndex].active) return neighbors;
    
    float entityX = m_allEntities[entityIndex].x;
    float entityY = m_allEntities[entityIndex].y;
    
    // Include the point itself per the standard definition |N_eps(p)| >= MinPts
    neighbors.push_back(entityIndex);
    
    for (int i = 0; i < static_cast<int>(m_allEntities.size()); i++) {
        if (i == entityIndex || !m_allEntities[i].active) continue;
        
        float distance = calculateDistance(entityX, entityY, m_allEntities[i].x, m_allEntities[i].y);
        if (distance <= m_dbscanEps) {
            neighbors.push_back(i);
        }
    }
    
    return neighbors;
}

void PartitionVisualization::updateDBSCANEntityColors() {
    // Reset all entities to default color first
    for (auto& entity : m_allEntities) {
        entity.color = getEntityColor(entity.id);
    }
    
    // Color entities based on their cluster assignments
    for (int i = 0; i < static_cast<int>(m_allEntities.size()); i++) {
        if (!m_allEntities[i].active) continue;
        
        int label = m_dbscanEntityLabels[i];
        if (label >= 0) {
            // Find the cluster this entity belongs to
            for (const auto& cluster : m_dbscanClusters) {
                if (cluster.clusterId == label) {
                    m_allEntities[i].color = cluster.color;
                    break;
                }
            }
        } else if (label == DBSCAN_NOISE) {
            // Noise points - make them gray
            m_allEntities[i].color = ImVec4(0.5f, 0.5f, 0.5f, 0.8f);
        }
    }
}

void PartitionVisualization::resetDBSCANLabels() {
    m_dbscanEntityLabels.clear();
    m_dbscanEntityLabels.resize(m_allEntities.size(), DBSCAN_UNVISITED);
}

ImVec4 PartitionVisualization::getDBSCANClusterColor(int clusterIndex) {
    static std::vector<ImVec4> colors = {
        ImVec4(1.0f, 0.0f, 0.0f, 0.8f), // Red
        ImVec4(0.0f, 1.0f, 0.0f, 0.8f), // Green
        ImVec4(0.0f, 0.0f, 1.0f, 0.8f), // Blue
        ImVec4(1.0f, 1.0f, 0.0f, 0.8f), // Yellow
        ImVec4(1.0f, 0.0f, 1.0f, 0.8f), // Magenta
        ImVec4(0.0f, 1.0f, 1.0f, 0.8f), // Cyan
        ImVec4(1.0f, 0.5f, 0.0f, 0.8f), // Orange
        ImVec4(0.5f, 0.0f, 1.0f, 0.8f), // Purple
        ImVec4(0.8f, 0.2f, 0.2f, 0.8f), // Dark Red
        ImVec4(0.2f, 0.8f, 0.2f, 0.8f)  // Dark Green
    };
    
    return colors[clusterIndex % colors.size()];
}

void PartitionVisualization::renderDBSCANClusters() {
    for (const auto& cluster : m_dbscanClusters) {
        if (cluster.entityIndices.empty()) continue;
        
        // Draw connections between entities in the same cluster
        for (size_t i = 0; i < cluster.entityIndices.size(); i++) {
            for (size_t j = i + 1; j < cluster.entityIndices.size(); j++) {
                int entityIndex1 = cluster.entityIndices[i];
                int entityIndex2 = cluster.entityIndices[j];
                
                if (entityIndex1 < static_cast<int>(m_allEntities.size()) && 
                    entityIndex2 < static_cast<int>(m_allEntities.size()) &&
                    m_allEntities[entityIndex1].active && m_allEntities[entityIndex2].active) {
                    
                    // Only draw line if entities are within epsilon distance
                    float distance = calculateDistance(
                        m_allEntities[entityIndex1].x, m_allEntities[entityIndex1].y,
                        m_allEntities[entityIndex2].x, m_allEntities[entityIndex2].y
                    );
                    if (distance <= m_dbscanEps) {
                        double x[] = {m_allEntities[entityIndex1].x, m_allEntities[entityIndex2].x};
                        double y[] = {m_allEntities[entityIndex1].y, m_allEntities[entityIndex2].y};
                        
                        ImVec4 lineColor = cluster.color;
                        lineColor.w = 0.6f; // Make lines semi-transparent
                        
                        ImPlot::SetNextLineStyle(lineColor, 1.0f);
                        ImPlot::PlotLine("DBSCANConnection", x, y, 2);
                    }
                }
            }
        }

        // Draw boundary: Voronoi cells or Convex hulls depending on toggle
        if (m_useVoronoi) {
            // Compute Voronoi cell for this cluster
            std::vector<ImVec2> cell;
            calculateDBSCANVoronoiCell(cluster.clusterId, cell);
            
            if (cell.size() >= 3) {
                std::vector<double> x, y;
                for (const auto& point : cell) {
                    x.push_back(point.x);
                    y.push_back(point.y);
                }
                x.push_back(cell[0].x);
                y.push_back(cell[0].y);
                
                ImVec4 cellColor = cluster.color;
                cellColor.w = 0.3f; // Semi-transparent
                ImPlot::SetNextLineStyle(cellColor, 2.0f);
                ImPlot::PlotLine("DBSCANVoronoiCell", x.data(), y.data(), static_cast<int>(x.size()));
            }
        } else {
            // Draw convex hull around cluster
            std::vector<Entity*> clusterEntities;
            for (int entityIndex : cluster.entityIndices) {
                if (entityIndex < static_cast<int>(m_allEntities.size()) && m_allEntities[entityIndex].active) {
                    clusterEntities.push_back(&m_allEntities[entityIndex]);
                }
            }
            
            if (clusterEntities.size() >= 3) {
                std::vector<ImVec2> hull;
                calculateConvexHull(clusterEntities, hull);
                
                if (hull.size() >= 3) {
                    std::vector<double> x, y;
                    for (const auto& point : hull) {
                        x.push_back(point.x);
                        y.push_back(point.y);
                    }
                    x.push_back(hull[0].x);
                    y.push_back(hull[0].y);
                    
                    ImVec4 hullColor = cluster.color;
                    hullColor.w = 0.3f; // Semi-transparent
                    ImPlot::SetNextLineStyle(hullColor, 2.0f);
                    ImPlot::PlotLine("DBSCANConvexHull", x.data(), y.data(), static_cast<int>(x.size()));
                }
            }
        }
    }
}

void PartitionVisualization::calculateDBSCANVoronoiCell(int clusterId, std::vector<ImVec2>& cell) {
    // Find the cluster
    DBSCANCluster* cluster = nullptr;
    for (auto& c : m_dbscanClusters) {
        if (c.clusterId == clusterId) {
            cluster = &c;
            break;
        }
    }
    
    if (!cluster || cluster->entityIndices.empty()) return;
    
    // Calculate cluster centroid
    float centroidX = 0.0f, centroidY = 0.0f;
    int validEntities = 0;
    for (int entityId : cluster->entityIndices) {
        for (const auto& entity : m_allEntities) {
            if (entity.id == entityId && entity.active) {
                centroidX += entity.x;
                centroidY += entity.y;
                validEntities++;
                break;
            }
        }
    }
    
    if (validEntities > 0) {
        centroidX /= validEntities;
        centroidY /= validEntities;
    }
    
    // Collect all cluster centroids as Voronoi sites
    std::vector<ImVec2> allSites;
    for (const auto& c : m_dbscanClusters) {
        // Calculate centroid for each cluster
        float cx = 0.0f, cy = 0.0f;
        int count = 0;
        for (int entityId : c.entityIndices) {
            for (const auto& entity : m_allEntities) {
                if (entity.id == entityId && entity.active) {
                    cx += entity.x;
                    cy += entity.y;
                    count++;
                    break;
                }
            }
        }
        if (count > 0) {
            cx /= count;
            cy /= count;
            allSites.push_back({cx, cy});
        }
    }
    
    // Use world bounds for Voronoi cell calculation
    ImVec2 boundsCenter = {(m_worldMinX + m_worldMaxX) * 0.5f, (m_worldMinY + m_worldMaxY) * 0.5f};
    ImVec2 boundsSize = {m_worldMaxX - m_worldMinX, m_worldMaxY - m_worldMinY};
    
    cell = computeVoronoiCell({centroidX, centroidY}, allSites, boundsCenter, boundsSize);
}

void PartitionVisualization::updateDBSCANIfNeeded(float dt) {
    m_dbscanUpdateTimer += dt;
    
    if (m_dbscanUpdateTimer >= m_dbscanUpdateInterval) {
        // Perform a quick DBSCAN update
        performDBSCANClustering();
        m_dbscanUpdateTimer = 0.0f;
    }
}

// Partition visualization helpers
void PartitionVisualization::calculatePartitionConvexHull(const Partition& partition, std::vector<ImVec2>& hull) {
    if (partition.entities.size() < 3) return;
    
    hull.clear();
    
    // Convert partition entities to ImVec2 points
    std::vector<ImVec2> points;
    for (const auto& entity : partition.entities) {
        points.push_back({entity.x, entity.y});
    }
    
    // Find the bottom-most point (and leftmost in case of tie)
    int bottomIndex = 0;
    for (size_t i = 1; i < points.size(); i++) {
        if (points[i].y < points[bottomIndex].y || 
            (points[i].y == points[bottomIndex].y && points[i].x < points[bottomIndex].x)) {
            bottomIndex = static_cast<int>(i);
        }
    }
    
    // Sort points by polar angle with respect to bottom point
    std::vector<std::pair<float, int>> angles;
    for (size_t i = 0; i < points.size(); i++) {
        if (i == bottomIndex) continue;
        
        float dx = points[i].x - points[bottomIndex].x;
        float dy = points[i].y - points[bottomIndex].y;
        float angle = std::atan2(dy, dx);
        angles.push_back({angle, static_cast<int>(i)});
    }
    
    std::sort(angles.begin(), angles.end());
    
    // Build convex hull using Graham scan
    hull.push_back(points[bottomIndex]);
    
    for (const auto& anglePair : angles) {
        int pointIndex = anglePair.second;
        hull.push_back(points[pointIndex]);
        
        // Remove points that make a non-left turn
        while (hull.size() > 2) {
            ImVec2 p1 = hull[hull.size() - 3];
            ImVec2 p2 = hull[hull.size() - 2];
            ImVec2 p3 = hull[hull.size() - 1];
            
            // Calculate cross product to determine turn direction
            float cross = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
            if (cross <= 0) {
                // Remove middle point (non-left turn)
                hull.erase(hull.end() - 2);
            } else {
                break;
            }
        }
    }
}

void PartitionVisualization::calculatePartitionVoronoiCell(const Partition& partition, std::vector<ImVec2>& cell) {
    if (partition.entities.empty()) return;
    
    // Calculate partition centroid
    float centroidX = 0.0f, centroidY = 0.0f;
    for (const auto& entity : partition.entities) {
        centroidX += entity.x;
        centroidY += entity.y;
    }
    centroidX /= partition.entities.size();
    centroidY /= partition.entities.size();
    
    // Collect all partition centroids as Voronoi sites
    std::vector<ImVec2> allSites;
    for (const auto& p : m_partitions) {
        if (p.entities.empty()) continue;
        
        float cx = 0.0f, cy = 0.0f;
        for (const auto& entity : p.entities) {
            cx += entity.x;
            cy += entity.y;
        }
        cx /= p.entities.size();
        cy /= p.entities.size();
        allSites.push_back({cx, cy});
    }
    
    // Use world bounds for Voronoi cell calculation
    ImVec2 boundsCenter = {(m_worldMinX + m_worldMaxX) * 0.5f, (m_worldMinY + m_worldMaxY) * 0.5f};
    ImVec2 boundsSize = {m_worldMaxX - m_worldMinX, m_worldMaxY - m_worldMinY};
    
    cell = computeVoronoiCell({centroidX, centroidY}, allSites, boundsCenter, boundsSize);
}

void PartitionVisualization::generatePartitions() {
    switch (m_currentMethod) {
        case PartitioningMethod::QUADTREE:
            generateQuadtreePartitions();
            break;
        case PartitioningMethod::KD_TREE:
            generateKdTreePartitions();
            break;
    }
}

void PartitionVisualization::generateKdTreePartitions() {
    // Clear existing partitions
    m_partitions.clear();
    
    // Build KD-tree
    buildKdTree();
    
    // Collect partitions from KD-tree (only those with entities)
    collectKdTreePartitions(m_kdtree.get());
}

void PartitionVisualization::updateEntityMovement(float dt) {
    for (auto& entity : m_allEntities) {
        if (!entity.active) continue;
        
        // Update position
        entity.x += entity.velocityX * dt;
        entity.y += entity.velocityY * dt;
        
        // Handle boundary collision
        handleEntityCollision(entity);
    }
}

void PartitionVisualization::updatePartitionsIfNeeded(float dt) {
    m_movementUpdateTimer += dt;
    
    if (m_movementUpdateTimer >= m_movementUpdateInterval) {
        // Regenerate partitions when entities have moved significantly
        generatePartitions();
        m_movementUpdateTimer = 0.0f;
    }
}

void PartitionVisualization::handleEntityCollision(Entity& entity) {
    // Simple boundary collision - reverse velocity if hitting world bounds
    if (entity.x < m_worldMinX || entity.x > m_worldMaxX) {
        entity.velocityX = -entity.velocityX;
        entity.x = std::max(m_worldMinX, std::min(m_worldMaxX, entity.x));
    }
    if (entity.y < m_worldMinY || entity.y > m_worldMaxY) {
        entity.velocityY = -entity.velocityY;
        entity.y = std::max(m_worldMinY, std::min(m_worldMaxY, entity.y));
    }
}

void PartitionVisualization::renderQuadtreeSquares(QuadtreeNode* node) {
    if (!node) return;
    
    // Draw this node's boundary
    ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 0.3f);
    ImPlot::SetNextLineStyle(color, 1.0f);
    double x[] = {node->minX, node->maxX, node->maxX, node->minX, node->minX};
    double y[] = {node->minY, node->minY, node->maxY, node->maxY, node->minY};
    ImPlot::PlotLine("QuadtreeSquare", x, y, 5);
    
    // Recursively draw children
    if (!node->isLeaf) {
        renderQuadtreeSquares(node->topLeft.get());
        renderQuadtreeSquares(node->topRight.get());
        renderQuadtreeSquares(node->bottomLeft.get());
        renderQuadtreeSquares(node->bottomRight.get());
    }
}


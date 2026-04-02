#pragma once
#include "DebugView.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "atlasnet/core/heuristic/KDTree/KDTree.hpp"
#include "raylib.h"
#include <algorithm>
#include <atomic>
#include <execution>
#include <gtest/gtest.h>
#include <string>
#include <vector>
TEST(KDTree2D, Basic)
{

  std::vector<vec2> points;
  uint32_t GenPoints = 1000;
  float SceneScale = 100.0f;
  uint32_t DesiredRegionCount = 3;
  // gen points from - SceneScale to SceneScale

  for (uint32_t i = 0; i < GenPoints; ++i)
  {
    float x =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * SceneScale - SceneScale;
    float y =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * SceneScale - SceneScale;
    points.push_back({x, y});
  }

  AtlasNet::KDTreeHeuristic2f tree;
  AtlasNet::SimpleRegionIdGenerator idGen;
  std::unique_ptr<AtlasNet::IPartitionT<2, float>> partition =
      tree.Partition(std::span(points), DesiredRegionCount, idGen);

  AtlasNet::KDTreePartition2f& kdPartition =
      dynamic_cast<AtlasNet::KDTreePartition2f&>(*partition);

  DebugView view(DebugView::Scene2DSettings{.SceneScale = SceneScale * 2.0f});
  view.on_update(
          [&](DebugView::Context& ctx)
          {
            for (const auto& region : kdPartition.Regions())
            {
              std::string text = std::format(" region {}", region.id);
              DrawText(text.c_str(), int(region.centroid[0]),
                       int(region.centroid[1]), 1, DARKGRAY);

              ctx.DrawAABB(region.bounds, DebugView::DrawMode::Wireframe, BLUE);
            }
            for (const auto& point : points)
            {
              Vector2 p = {point[0], point[1]};
              DrawCircleV(p, 0.5f, RED);
            }
          })
      .run();

  EXPECT_EQ(kdPartition.RegionCount(), DesiredRegionCount);

  std::atomic_bool foundInvalidRegion = false;
  std::atomic_uint64_t LowestRegionCount = std::numeric_limits<uint64_t>::max(),
                       HighestRegionCount = 0;
  std::for_each(std::execution::par, kdPartition.Regions().begin(),
                kdPartition.Regions().end(),
                [&](const AtlasNet::KDTreePartition2f::RegionType& region)
                {
                  LowestRegionCount = std::min(
                      LowestRegionCount.load(),
                      static_cast<uint64_t>(region.pointIndices.size()));
                  HighestRegionCount = std::max(
                      HighestRegionCount.load(),
                      static_cast<uint64_t>(region.pointIndices.size()));
                  if (region.pointIndices.empty())
                  {
                    foundInvalidRegion = true;
                  }
                });

  EXPECT_FALSE(foundInvalidRegion);

  std::atomic_bool foundInvalidQuery = false;
  std::for_each(std::execution::par, points.begin(), points.end(),
                [&](const vec2& point)
                {
                  auto regionIdOpt = kdPartition.QueryRegion(point);
                  if (!regionIdOpt)
                  {
                    foundInvalidQuery = true;
                  }
                  else
                  {
                    auto regionId = *regionIdOpt;
                    const auto* region = kdPartition.FindRegion(regionId);

                    if (!region || !region->bounds.contains(point))
                    {
                      foundInvalidQuery = true;
                    }
                  }
                });
  EXPECT_FALSE(foundInvalidQuery);
  float DisparityRatio =
      static_cast<float>(HighestRegionCount) / LowestRegionCount;
  EXPECT_LE(DisparityRatio, 2.0f);
}
TEST(KDTree2D, Sequential)
{

  std::vector<vec2> points;
  uint32_t GenPoints = 1000;
  float SceneScale = 100.0f;
  uint32_t DesiredRegionMaxCount = 30;
  // gen points from - SceneScale to SceneScale
  float MaxRadius = SceneScale * 0.5f;
  for (uint32_t i = 0; i < GenPoints; ++i)
  {
    float RandDist = std::sqrt(static_cast<float>(rand()) / RAND_MAX);
    float RandAngle =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159265f;
    float Radius = RandDist * MaxRadius;
    float x = std::cos(RandAngle) * Radius;
    float y = std::sin(RandAngle) * Radius;
    points.push_back({x, y});
  }

  AtlasNet::KDTreeHeuristic2f tree;
  AtlasNet::SimpleRegionIdGenerator idGen;
  std::unique_ptr<AtlasNet::IPartitionT<2, float>> partition =
      tree.Partition(std::span(points), 1, idGen);

  AtlasNet::KDTreePartition2f* kdPartition =
      dynamic_cast<AtlasNet::KDTreePartition2f*>(partition.get());

  DebugView view(DebugView::Scene2DSettings{.SceneScale = SceneScale * 2.0f});
  view.on_update(
          [&](DebugView::Context& ctx)
          {
            const float TimePerSplit = 0.25f;

            // Up: 1 -> DesiredRegionMaxCount
            for (int i = 2; i <= static_cast<int>(DesiredRegionMaxCount); ++i)
            {
              ctx.timer(std::format("split_up_{}", i), TimePerSplit * (i - 1))
                  .on_first_active(
                      [&]()
                      {
                        std::cerr << "Repartitioned to " << i
                                  << " regions at time " << ctx.totalTime
                                  << std::endl;
                        partition = tree.Repartition(std::span(points), i,
                                                     *kdPartition, idGen);
                        kdPartition = dynamic_cast<AtlasNet::KDTreePartition2f*>(
                            partition.get());

                            EXPECT_EQ(kdPartition->RegionCount(), i);
                      });
                      
            }

            // Down: DesiredRegionMaxCount-1 -> 1
            const float UpPhaseEndTime =
                TimePerSplit * (static_cast<float>(DesiredRegionMaxCount) - 1.0f);

            for (int i = static_cast<int>(DesiredRegionMaxCount) - 1; i >= 1; --i)
            {
              const float t = UpPhaseEndTime +
                              TimePerSplit *
                                  (static_cast<float>(DesiredRegionMaxCount - i));

              ctx.timer(std::format("split_down_{}", i), t)
                  .on_first_active(
                      [&]()
                      {
                        std::cerr << "Repartitioned to " << i
                                  << " regions at time " << ctx.totalTime
                                  << std::endl;
                        partition = tree.Repartition(std::span(points), i,
                                                     *kdPartition, idGen);
                        kdPartition = dynamic_cast<AtlasNet::KDTreePartition2f*>(
                            partition.get());
                            EXPECT_EQ(kdPartition->RegionCount(), i);
                      });
            }

            for (const auto& region : kdPartition->Regions())
            {
              std::string text = std::format(" region {}", region.id);
              DrawText(text.c_str(), int(region.centroid[0]),
                       int(region.centroid[1]), 1, DARKGRAY);

              ctx.DrawAABB(region.bounds, DebugView::DrawMode::Wireframe, BLUE);
            }
            for (const auto& point : points)
            {
              Vector2 p = {point[0], point[1]};
              DrawCircleV(p, 0.5f, RED);
            }
          })
      .run();
}

TEST(KDTree3D, Basic)
{

  std::vector<vec3> points;
  uint32_t GenPoints = 1000;
  float SceneScale = 10.0f;
  uint32_t DesiredRegionCount = 3;
  // gen points from - SceneScale to SceneScale

  for (uint32_t i = 0; i < GenPoints; ++i)
  {
    float x =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * SceneScale - SceneScale;
    float y =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * SceneScale - SceneScale;
    float z =
        static_cast<float>(rand()) / RAND_MAX * 2.0f * SceneScale - SceneScale;
    points.push_back({x, y, z});
  }

  AtlasNet::KDTreeHeuristic3f tree;
  AtlasNet::SimpleRegionIdGenerator idGen;
  std::unique_ptr<AtlasNet::IPartitionT<3, float>> partition =
      tree.Partition(std::span(points), DesiredRegionCount, idGen);

  AtlasNet::KDTreePartition3f& kdPartition =
      dynamic_cast<AtlasNet::KDTreePartition3f&>(*partition);

  DebugView view(DebugView::Scene3DSettings{.SceneScale = SceneScale * 2.0f});
  view.on_update(
          [&](DebugView::Context& ctx)
          {
            for (const auto& region : kdPartition.Regions())
            {
              ctx.DrawAABB(region.bounds, DebugView::DrawMode::Wireframe, BLUE);
            }
            for (const auto& point : points)
            {
              Vector3 p = {point[0], point[1], point[2]};
              DrawSphere(p, 0.05f, RED);
            }
          })
      .on_update_post_3d(
          [&](DebugView::Context& ctx)
          {
            for (const auto& region : kdPartition.Regions())
            {
              std::string text = std::format(" region {}", region.id);
              vec2 screenPos = ctx.world_to_screen(region.centroid);
              DrawText(text.c_str(), int(screenPos.x), int(screenPos.y), 3,
                       DARKGRAY);
            }
          })
      .run();

  EXPECT_EQ(kdPartition.RegionCount(), DesiredRegionCount);

  std::atomic_uint64_t LowestRegionCount = std::numeric_limits<uint64_t>::max(),
                       HighestRegionCount = 0;
  std::atomic_bool foundInvalidRegion = false;
  std::for_each(std::execution::par, kdPartition.Regions().begin(),
                kdPartition.Regions().end(),
                [&](const AtlasNet::KDTreePartition3f::RegionType& region)
                {
                  if (region.pointIndices.empty())
                  {
                    foundInvalidRegion = true;
                  }
                  LowestRegionCount = std::min(
                      LowestRegionCount.load(),
                      static_cast<uint64_t>(region.pointIndices.size()));
                  HighestRegionCount = std::max(
                      HighestRegionCount.load(),
                      static_cast<uint64_t>(region.pointIndices.size()));
                });

  EXPECT_FALSE(foundInvalidRegion);

  std::atomic_bool foundInvalidQuery = false;
  std::for_each(std::execution::par, points.begin(), points.end(),
                [&](const vec3& point)
                {
                  auto regionIdOpt = kdPartition.QueryRegion(point);
                  if (!regionIdOpt)
                  {
                    foundInvalidQuery = true;
                  }
                  else
                  {
                    auto regionId = *regionIdOpt;
                    const auto* region = kdPartition.FindRegion(regionId);

                    if (!region || !region->bounds.contains(point))
                    {
                      foundInvalidQuery = true;
                    }
                  }
                });
  EXPECT_FALSE(foundInvalidQuery);
  float DisparityRatio =
      static_cast<float>(HighestRegionCount) / LowestRegionCount;
  EXPECT_LE(DisparityRatio, 2.0f);
}
TEST(KDTree3D, Sequential)
{
  std::vector<vec3> points;
  uint32_t GenPoints = 1000;
  float SceneScale = 10.0f;
  uint32_t DesiredRegionMaxCount = 30;

  // Generate points uniformly inside a sphere of radius SceneScale
  const float MaxRadius = SceneScale * 0.5f;
  for (uint32_t i = 0; i < GenPoints; ++i)
  {
    float u = static_cast<float>(rand()) / RAND_MAX; // [0,1]
    float v = static_cast<float>(rand()) / RAND_MAX; // [0,1]
    float w = static_cast<float>(rand()) / RAND_MAX; // [0,1]

    float theta = 2.0f * 3.14159265f * u;
    float phi = std::acos(1.0f - 2.0f * v);
    float r = MaxRadius * std::cbrt(w);

    float sinPhi = std::sin(phi);
    float x = r * sinPhi * std::cos(theta);
    float y = r * sinPhi * std::sin(theta);
    float z = r * std::cos(phi);

    points.push_back({x, y, z});
  }

  AtlasNet::KDTreeHeuristic3f tree;
  AtlasNet::SimpleRegionIdGenerator idGen;
  std::unique_ptr<AtlasNet::IPartitionT<3, float>> partition =
      tree.Partition(std::span(points), 1, idGen);

  AtlasNet::KDTreePartition3f* kdPartition =
      dynamic_cast<AtlasNet::KDTreePartition3f*>(partition.get());

  DebugView view(DebugView::Scene3DSettings{.SceneScale = SceneScale * 2.0f});
  view.on_update(
          [&](DebugView::Context& ctx)
          {
            const float TimePerSplit = 0.25f;

            // Up: 1 -> DesiredRegionMaxCount
            for (int i = 2; i <= static_cast<int>(DesiredRegionMaxCount); ++i)
            {
              ctx.timer(std::format("split3d_up_{}", i), TimePerSplit * (i - 1))
                  .on_first_active(
                      [&]()
                      {
                        std::cerr << "[3D] Repartitioned to " << i
                                  << " regions at time " << ctx.totalTime
                                  << std::endl;
                        partition = tree.Repartition(std::span(points), i,
                                                     *kdPartition, idGen);
                        kdPartition = dynamic_cast<AtlasNet::KDTreePartition3f*>(
                            partition.get());
                        EXPECT_EQ(kdPartition->RegionCount(), static_cast<uint32_t>(i));
                      });
            }

            // Down: DesiredRegionMaxCount-1 -> 1
            const float UpPhaseEndTime =
                TimePerSplit * (static_cast<float>(DesiredRegionMaxCount) - 1.0f);

            for (int i = static_cast<int>(DesiredRegionMaxCount) - 1; i >= 1; --i)
            {
              const float t = UpPhaseEndTime +
                              TimePerSplit *
                                  (static_cast<float>(DesiredRegionMaxCount - i));

              ctx.timer(std::format("split3d_down_{}", i), t)
                  .on_first_active(
                      [&]()
                      {
                        std::cerr << "[3D] Repartitioned to " << i
                                  << " regions at time " << ctx.totalTime
                                  << std::endl;
                        partition = tree.Repartition(std::span(points), i,
                                                     *kdPartition, idGen);
                        kdPartition = dynamic_cast<AtlasNet::KDTreePartition3f*>(
                            partition.get());
                        EXPECT_EQ(kdPartition->RegionCount(), static_cast<uint32_t>(i));
                      });
            }

            for (const auto& region : kdPartition->Regions())
            {
              ctx.DrawAABB(region.bounds, DebugView::DrawMode::Wireframe, BLUE);
            }
            for (const auto& point : points)
            {
              Vector3 p = {point[0], point[1], point[2]};
              DrawSphere(p, 0.05f, RED);
            }
          })
      .on_update_post_3d(
          [&](DebugView::Context& ctx)
          {
            for (const auto& region : kdPartition->Regions())
            {
              std::string text = std::format(" region {}", region.id);
              vec2 screenPos = ctx.world_to_screen(region.centroid);
              DrawText(text.c_str(), int(screenPos.x), int(screenPos.y), 3,
                       DARKGRAY);
            }
          })
      .run();
}
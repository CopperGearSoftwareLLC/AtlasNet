#pragma once

#include "atlasnet/core/geometry/AABB.hpp"
#include "atlasnet/core/geometry/Vec.hpp"

#include "atlasnet/core/heuristic/IHeuristic.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>
namespace AtlasNet
{

template <std::size_t Dim, typename T>
  requires SupportedPartitionDimension<Dim, T>
class KDTreeHeuristic;

template <std::size_t Dim, typename T = float>
requires SupportedPartitionDimension<Dim, T>
class KDTreePartition final : public IPartitionT<Dim, T>
{
public:
  using VecType = vec<Dim, T>;
  using BoxType = AABB<Dim, T>;
  using RegionType = RegionT<Dim, T>;

private:
  struct Node
  {
    BoxType bounds{};
    bool isLeaf = false;

    std::size_t splitAxis = 0;
    T splitValue = T(0);

    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    std::size_t regionIndex = InvalidRegionIndex;
  };

  static constexpr std::size_t InvalidRegionIndex =
      std::numeric_limits<std::size_t>::max();

public:
  KDTreePartition() = default;

  [[nodiscard]] std::size_t RegionCount() const override
  {
    return regions_.size();
  }

  [[nodiscard]] const RegionType* FindRegion(RegionID id) const override
  {
    auto it = regionLookup_.find(id);
    if (it == regionLookup_.end())
      return nullptr;
    return &regions_[it->second];
  }

  [[nodiscard]] std::span<const RegionType> Regions() const override
  {
    return regions_;
  }

  [[nodiscard]] std::optional<RegionID>
  QueryRegion(const VecType& point) const override
  {
    if (!root_)
      return std::nullopt;

    if (!Contains(root_->bounds, point))
      return std::nullopt;

    const Node* node = root_.get();
    while (node && !node->isLeaf)
    {
      node = (point[node->splitAxis] <= node->splitValue)
                 ? node->left.get()
                 : node->right.get();
    }

    if (!node || node->regionIndex == InvalidRegionIndex)
      return std::nullopt;

    return regions_[node->regionIndex].id;
  }

  [[nodiscard]] RegionID QueryClosestRegion(const VecType& point) const override
  {
    assert(!regions_.empty() && "QueryClosestRegion called on empty partition");

    std::size_t bestIndex = 0;
    T bestDist2 = DistanceSquared(point, regions_[0].centroid);

    for (std::size_t i = 1; i < regions_.size(); ++i)
    {
      const T d2 = DistanceSquared(point, regions_[i].centroid);
      if (d2 < bestDist2)
      {
        bestDist2 = d2;
        bestIndex = i;
      }
    }

    return regions_[bestIndex].id;
  }

private:
  friend class KDTreeHeuristic<Dim, T>;

  static bool Contains(const BoxType& box, const VecType& p)
  {
    for (std::size_t axis = 0; axis < Dim; ++axis)
    {
      if (p[axis] < box.min[axis] || p[axis] > box.max[axis])
        return false;
    }
    return true;
  }

  static T DistanceSquared(const VecType& a, const VecType& b)
  {
    T sum = T(0);
    for (std::size_t axis = 0; axis < Dim; ++axis)
    {
      const T d = a[axis] - b[axis];
      sum += d * d;
    }
    return sum;
  }

  static VecType ZeroVec()
  {
    VecType v{};
    for (std::size_t i = 0; i < Dim; ++i)
      v[i] = T(0);
    return v;
  }

  static VecType ComputeCentroid(std::span<const VecType> points,
                                 const std::vector<std::size_t>& indices)
  {
    if (indices.empty())
      return ZeroVec();

    VecType sum = ZeroVec();
    for (std::size_t idx : indices)
      sum += points[idx];

    return sum / static_cast<T>(indices.size());
  }

  static BoxType ComputeTightBounds(std::span<const VecType> points,
                                    const std::vector<std::size_t>& indices)
  {
    BoxType box{};

    if (indices.empty())
    {
      box.min = ZeroVec();
      box.max = ZeroVec();
      return box;
    }

    box.min = points[indices.front()];
    box.max = points[indices.front()];

    for (std::size_t idx : indices)
    {
      const VecType& p = points[idx];
      for (std::size_t axis = 0; axis < Dim; ++axis)
      {
        box.min[axis] = std::min(box.min[axis], p[axis]);
        box.max[axis] = std::max(box.max[axis], p[axis]);
      }
    }

    return box;
  }

  static BoxType MakeChildBounds(const BoxType& parent, std::size_t splitAxis,
                                 T splitValue, bool makeLeftChild)
  {
    BoxType child = parent;
    if (makeLeftChild)
      child.max[splitAxis] = splitValue;
    else
      child.min[splitAxis] = splitValue;
    return child;
  }

  std::vector<RegionType> regions_;
  std::unordered_map<RegionID, std::size_t> regionLookup_;
  std::unique_ptr<Node> root_;
};

template <std::size_t Dim, typename T>
requires SupportedPartitionDimension<Dim, T>
class KDTreeHeuristic final : public IHeuristicT<Dim, T>
{
public:
  using VecType = vec<Dim, T>;
  using BoxType = AABB<Dim, T>;
  using PartitionBase = IPartitionT<Dim, T>;
  using RegionType = RegionT<Dim, T>;
  using PartitionImpl = KDTreePartition<Dim, T>;
  using Node = typename PartitionImpl::Node;

  [[nodiscard]] std::unique_ptr<PartitionBase>
  Partition(std::span<const VecType> points, std::size_t desiredRegionCount,
            IRegionIdGenerator& ids) const override
  {
    auto result = std::make_unique<PartitionImpl>();

    if (points.empty() || desiredRegionCount == 0)
      return result;

    const std::size_t actualRegionCount =
        std::min(desiredRegionCount, points.size());

    std::vector<std::size_t> indices(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
      indices[i] = i;

    const BoxType rootBounds =
        PartitionImpl::ComputeTightBounds(points, indices);

    result->root_ = BuildRecursive(points, indices, actualRegionCount,
                                   rootBounds, ids, result->regions_);

    BuildRegionLookup(*result);
    return result;
  }

  [[nodiscard]] std::unique_ptr<PartitionBase>
  Repartition(std::span<const VecType> points, std::size_t desiredRegionCount,
              const PartitionBase& previous, IRegionIdGenerator& ids,
              const RepartitionOptions& options = {}) const override
  {
    auto result = std::make_unique<PartitionImpl>();

    if (points.empty() || desiredRegionCount == 0)
      return result;

    const std::size_t actualRegionCount =
        std::min(desiredRegionCount, points.size());

    std::vector<std::size_t> indices(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
      indices[i] = i;

    const BoxType rootBounds =
        PartitionImpl::ComputeTightBounds(points, indices);

    std::vector<RegionID> reusableIds =
        CollectReusableRegionIds(previous, actualRegionCount, options);

    std::size_t reuseCursor = 0;

    result->root_ = BuildRecursive(points, indices, actualRegionCount,
                                   rootBounds, ids, result->regions_,
                                   &reusableIds, &reuseCursor);

    ApplyStableIdRemap(previous, options, *result, ids);
    BuildRegionLookup(*result);
    return result;
  }

private:
  static void BuildRegionLookup(PartitionImpl& partition)
  {
    partition.regionLookup_.clear();
    partition.regionLookup_.reserve(partition.regions_.size());

    for (std::size_t i = 0; i < partition.regions_.size(); ++i)
      partition.regionLookup_[partition.regions_[i].id] = i;
  }

  static std::vector<RegionID>
  CollectReusableRegionIds(const PartitionBase& previous,
                           std::size_t desiredCount,
                           const RepartitionOptions& options)
  {
    std::vector<RegionID> ids;
    const auto oldRegions = previous.Regions();

    if (!options.allowRegionRemoval)
    {
      ids.reserve(oldRegions.size());
      for (const auto& r : oldRegions)
        ids.push_back(r.id);
    }
    else
    {
      const std::size_t keepCount = std::min(desiredCount, oldRegions.size());
      ids.reserve(keepCount);
      for (std::size_t i = 0; i < keepCount; ++i)
        ids.push_back(oldRegions[i].id);
    }

    if (!options.allowNewRegions && ids.size() > desiredCount)
      ids.resize(desiredCount);

    return ids;
  }

  static void ApplyStableIdRemap(const PartitionBase& previous,
                                 const RepartitionOptions& options,
                                 PartitionImpl& current,
                                 IRegionIdGenerator& ids)
  {
    const auto oldRegions = previous.Regions();
    if (oldRegions.empty() || current.regions_.empty())
      return;

    const float stabilityWeight = std::max(0.0f, options.stabilityWeight);
    if (stabilityWeight <= 0.0f)
      return;

    std::vector<bool> oldUsed(oldRegions.size(), false);
    std::vector<RegionID> remapped(current.regions_.size(), 0);

    for (std::size_t i = 0; i < current.regions_.size(); ++i)
    {
      T bestScore = std::numeric_limits<T>::max();
      std::size_t bestOld = std::numeric_limits<std::size_t>::max();

      for (std::size_t j = 0; j < oldRegions.size(); ++j)
      {
        if (oldUsed[j])
          continue;

        const T d2 = PartitionImpl::DistanceSquared(current.regions_[i].centroid,
                                                    oldRegions[j].centroid);

        const T score = d2 / static_cast<T>(stabilityWeight);
        if (score < bestScore)
        {
          bestScore = score;
          bestOld = j;
        }
      }

      if (bestOld != std::numeric_limits<std::size_t>::max())
      {
        remapped[i] = oldRegions[bestOld].id;
        oldUsed[bestOld] = true;
      }
    }

    for (std::size_t i = 0; i < current.regions_.size(); ++i)
    {
      if (remapped[i] == 0)
        remapped[i] = ids.Next();

      current.regions_[i].id = remapped[i];
    }
  }

  static std::unique_ptr<Node>
  BuildRecursive(std::span<const VecType> points,
                 std::vector<std::size_t>& indices,
                 std::size_t regionCount,
                 const BoxType& cellBounds,
                 IRegionIdGenerator& ids,
                 std::vector<RegionType>& outRegions,
                 std::vector<RegionID>* reusableIds = nullptr,
                 std::size_t* reusableCursor = nullptr)
  {
    auto node = std::make_unique<Node>();
    node->bounds = cellBounds;

    if (regionCount <= 1 || indices.size() <= 1)
    {
      node->isLeaf = true;

      RegionType region;
      if (reusableIds && reusableCursor && *reusableCursor < reusableIds->size())
        region.id = (*reusableIds)[(*reusableCursor)++];
      else
        region.id = ids.Next();

      region.pointIndices = std::move(indices);
      region.centroid =
          PartitionImpl::ComputeCentroid(points, region.pointIndices);

      // This is the important change:
      // bounds are the full allotted kd-cell, not the tight point bounds.
      region.bounds = cellBounds;

      node->regionIndex = outRegions.size();
      outRegions.push_back(std::move(region));
      return node;
    }

    // Use tight point bounds only to choose a good split axis.
    const BoxType tightBounds =
        PartitionImpl::ComputeTightBounds(points, indices);

    node->splitAxis = ChooseSplitAxis(tightBounds);

    const std::size_t leftRegionCount = regionCount / 2;
    const std::size_t rightRegionCount = regionCount - leftRegionCount;

    std::sort(indices.begin(), indices.end(),
              [&](std::size_t a, std::size_t b)
              {
                const T va = points[a][node->splitAxis];
                const T vb = points[b][node->splitAxis];
                if (va != vb)
                  return va < vb;
                return a < b;
              });

    const std::size_t targetLeftPoints =
        (indices.size() * leftRegionCount) / regionCount;

    const std::size_t splitIndex =
        std::clamp<std::size_t>(targetLeftPoints, 1, indices.size() - 1);

    std::vector<std::size_t> leftIndices(indices.begin(),
                                         indices.begin() + splitIndex);
    std::vector<std::size_t> rightIndices(indices.begin() + splitIndex,
                                          indices.end());

    const T leftMax = points[leftIndices.back()][node->splitAxis];
    const T rightMin = points[rightIndices.front()][node->splitAxis];
    node->splitValue = (leftMax + rightMin) / static_cast<T>(2);

    const BoxType leftBounds =
        PartitionImpl::MakeChildBounds(cellBounds, node->splitAxis,
                                       node->splitValue, true);
    const BoxType rightBounds =
        PartitionImpl::MakeChildBounds(cellBounds, node->splitAxis,
                                       node->splitValue, false);

    node->left = BuildRecursive(points, leftIndices, leftRegionCount,
                                leftBounds, ids, outRegions,
                                reusableIds, reusableCursor);

    node->right = BuildRecursive(points, rightIndices, rightRegionCount,
                                 rightBounds, ids, outRegions,
                                 reusableIds, reusableCursor);

    return node;
  }

  static std::size_t ChooseSplitAxis(const BoxType& bounds)
  {
    std::size_t bestAxis = 0;
    T bestExtent = bounds.max[0] - bounds.min[0];

    for (std::size_t axis = 1; axis < Dim; ++axis)
    {
      const T extent = bounds.max[axis] - bounds.min[axis];
      if (extent > bestExtent)
      {
        bestExtent = extent;
        bestAxis = axis;
      }
    }

    return bestAxis;
  }
};

using KDTreePartition2f = KDTreePartition<2, float>;
using KDTreePartition3f = KDTreePartition<3, float>;
using KDTreeHeuristic2f = KDTreeHeuristic<2, float>;
using KDTreeHeuristic3f = KDTreeHeuristic<3, float>;

} // namespace AtlasNet
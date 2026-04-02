#pragma once
#include "atlasnet/core/geometry/Vec.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>
namespace AtlasNet
{

using RegionID = std::uint64_t;

template <std::size_t Dim, typename T>
concept SupportedPartitionDimension = (Dim == 2 || Dim == 3) && std::is_floating_point_v<T>;

template <std::size_t Dim, typename T = float>
requires SupportedPartitionDimension<Dim, T>
struct RegionT
{
  using VecType = vec<Dim, T>;

  RegionID id = 0;
  VecType centroid{T(0)};
  AABB<Dim, T> bounds{};
  std::vector<std::size_t> pointIndices;
};

template <std::size_t Dim, typename T = float>
requires SupportedPartitionDimension<Dim, T>
class IPartitionT
{
public:
  using VecType = vec<Dim, T>;
  using RegionType = RegionT<Dim, T>;

  virtual ~IPartitionT() = default;

  [[nodiscard]] virtual std::size_t RegionCount() const = 0;
  [[nodiscard]] virtual const RegionType* FindRegion(RegionID id) const = 0;
  [[nodiscard]] virtual std::span<const RegionType> Regions() const = 0;

  [[nodiscard]] virtual std::optional<RegionID>
  QueryRegion(const VecType& point) const = 0;

  [[nodiscard]] virtual RegionID
  QueryClosestRegion(const VecType& point) const = 0;
};

struct RepartitionOptions
{
  float stabilityWeight = 1.0f;
  bool allowNewRegions = true;
  bool allowRegionRemoval = true;
};

class IRegionIdGenerator
{
public:
  virtual ~IRegionIdGenerator() = default;
  virtual RegionID Next() = 0;
};

class SimpleRegionIdGenerator : public IRegionIdGenerator
{
  RegionID nextId = 1;
public:
  RegionID Next() override
  {    return nextId++;
  }
};

template <std::size_t Dim, typename T = float>
requires SupportedPartitionDimension<Dim, T>
class IHeuristicT
{
public:
  using VecType = vec<Dim, T>;
  using PartitionType = IPartitionT<Dim, T>;

  virtual ~IHeuristicT() = default;

  [[nodiscard]] virtual std::unique_ptr<PartitionType>
  Partition(std::span<const VecType> points, std::size_t desiredRegionCount,
            IRegionIdGenerator& ids) const = 0;

  [[nodiscard]] virtual std::unique_ptr<PartitionType>
  Repartition(std::span<const VecType> points, std::size_t desiredRegionCount,
              const PartitionType& previous, IRegionIdGenerator& ids,
              const RepartitionOptions& options = {}) const = 0;
};

// Optional aliases matching your original names for 3D.
using Region = RegionT<3, float>;
using IPartition = IPartitionT<3, float>;
using IHeuristic = IHeuristicT<3, float>;


} // namespace AtlasNet
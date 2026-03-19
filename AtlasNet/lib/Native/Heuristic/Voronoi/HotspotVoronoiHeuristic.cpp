#include "HotspotVoronoiHeuristic.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
constexpr double kTau = 6.28318530717958647692;

struct NormalizedPoint
{
	double x = 0.0;
	double y = 0.0;
};

struct PeakCandidate
{
	double score = 0.0;
	double x = 0.0;
	double y = 0.0;
};

[[nodiscard]] double Clamp01(const double value)
{
	return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double NormalizeCoordinate(
	const float value, const float minValue, const float maxValue)
{
	const double range =
		static_cast<double>(maxValue) - static_cast<double>(minValue);
	if (range <= 0.0)
	{
		return 0.5;
	}
	return Clamp01(
		(static_cast<double>(value) - static_cast<double>(minValue)) / range);
}

[[nodiscard]] float DenormalizeCoordinate(
	const double value, const float minValue, const float maxValue)
{
	return static_cast<float>(
		static_cast<double>(minValue) +
		Clamp01(value) *
			(static_cast<double>(maxValue) - static_cast<double>(minValue)));
}

[[nodiscard]] size_t ToGridIndex(
	const double normalized, const uint32_t gridResolution)
{
	if (gridResolution <= 1)
	{
		return 0;
	}
	const double scaled = Clamp01(normalized) * static_cast<double>(gridResolution);
	const auto idx = static_cast<size_t>(scaled);
	return std::min(idx, static_cast<size_t>(gridResolution - 1));
}

[[nodiscard]] std::vector<NormalizedPoint> NormalizeTransforms(
	const std::span<const AtlasTransform>& transforms,
	const HotspotVoronoiHeuristic::Options& options)
{
	std::vector<NormalizedPoint> points;
	points.reserve(transforms.size());

	const float minX = -options.NetHalfExtent.x;
	const float maxX = options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;
	const float maxY = options.NetHalfExtent.y;

	for (const AtlasTransform& transform : transforms)
	{
		points.push_back({
			.x = NormalizeCoordinate(transform.position.x, minX, maxX),
			.y = NormalizeCoordinate(transform.position.y, minY, maxY),
		});
	}
	return points;
}

[[nodiscard]] std::vector<PeakCandidate> FindPeakCandidates(
	const std::vector<NormalizedPoint>& points,
	const HotspotVoronoiHeuristic::Options& options)
{
	if (points.empty())
	{
		return {};
	}

	const uint32_t gridResolution = std::max<uint32_t>(8, options.DensityGridResolution);
	std::vector<double> counts(static_cast<size_t>(gridResolution) * gridResolution, 0.0);
	for (const auto& point : points)
	{
		const size_t x = ToGridIndex(point.x, gridResolution);
		const size_t y = ToGridIndex(point.y, gridResolution);
		counts[y * gridResolution + x] += 1.0;
	}

	std::vector<double> smoothed(counts.size(), 0.0);
	double maxScore = 0.0;
	const std::array<std::array<int, 3>, 3> kernel = {{
		{{1, 2, 1}},
		{{2, 4, 2}},
		{{1, 2, 1}},
	}};

	for (uint32_t y = 0; y < gridResolution; ++y)
	{
		for (uint32_t x = 0; x < gridResolution; ++x)
		{
			double score = 0.0;
			for (int ky = -1; ky <= 1; ++ky)
			{
				for (int kx = -1; kx <= 1; ++kx)
				{
					const int nx = static_cast<int>(x) + kx;
					const int ny = static_cast<int>(y) + ky;
					if (nx < 0 || ny < 0 ||
						nx >= static_cast<int>(gridResolution) ||
						ny >= static_cast<int>(gridResolution))
					{
						continue;
					}

					score += counts[static_cast<size_t>(ny) * gridResolution +
									static_cast<size_t>(nx)] *
							 static_cast<double>(
								 kernel[static_cast<size_t>(ky + 1)]
									   [static_cast<size_t>(kx + 1)]);
				}
			}
			score /= 16.0;
			smoothed[static_cast<size_t>(y) * gridResolution + x] = score;
			maxScore = std::max(maxScore, score);
		}
	}

	if (maxScore <= 0.0)
	{
		return {};
	}

	const double minPeakScore = std::max(1.0, maxScore * 0.15);
	std::vector<PeakCandidate> candidates;
	candidates.reserve(options.HotspotCount * 2);

	for (uint32_t y = 0; y < gridResolution; ++y)
	{
		for (uint32_t x = 0; x < gridResolution; ++x)
		{
			const double score = smoothed[static_cast<size_t>(y) * gridResolution + x];
			if (score < minPeakScore)
			{
				continue;
			}

			bool isLocalPeak = true;
			for (int ny = -1; ny <= 1 && isLocalPeak; ++ny)
			{
				for (int nx = -1; nx <= 1; ++nx)
				{
					if (nx == 0 && ny == 0)
					{
						continue;
					}

					const int px = static_cast<int>(x) + nx;
					const int py = static_cast<int>(y) + ny;
					if (px < 0 || py < 0 ||
						px >= static_cast<int>(gridResolution) ||
						py >= static_cast<int>(gridResolution))
					{
						continue;
					}

					if (smoothed[static_cast<size_t>(py) * gridResolution +
								 static_cast<size_t>(px)] > score)
					{
						isLocalPeak = false;
						break;
					}
				}
			}

			if (!isLocalPeak)
			{
				continue;
			}

			candidates.push_back({
				.score = score,
				.x = (static_cast<double>(x) + 0.5) /
					 static_cast<double>(gridResolution),
				.y = (static_cast<double>(y) + 0.5) /
					 static_cast<double>(gridResolution),
			});
		}
	}

	std::sort(candidates.begin(), candidates.end(),
			  [](const PeakCandidate& left, const PeakCandidate& right)
			  { return left.score > right.score; });

	std::vector<PeakCandidate> deduped;
	deduped.reserve(std::min<size_t>(candidates.size(), options.HotspotCount));
	const double minPeakDistance = 2.0 / static_cast<double>(gridResolution);
	const double minPeakDistanceSq = minPeakDistance * minPeakDistance;

	for (const PeakCandidate& candidate : candidates)
	{
		bool tooClose = false;
		for (const PeakCandidate& existing : deduped)
		{
			const double dx = candidate.x - existing.x;
			const double dy = candidate.y - existing.y;
			if ((dx * dx) + (dy * dy) < minPeakDistanceSq)
			{
				tooClose = true;
				break;
			}
		}
		if (tooClose)
		{
			continue;
		}
		deduped.push_back(candidate);
		if (deduped.size() >= options.HotspotCount)
		{
			break;
		}
	}

	return deduped;
}

[[nodiscard]] std::vector<HotspotVoronoiSample> BuildHotspots(
	const std::vector<NormalizedPoint>& points,
	const HotspotVoronoiHeuristic::Options& options)
{
	if (points.empty())
	{
		return {};
	}

	std::vector<PeakCandidate> peaks = FindPeakCandidates(points, options);
	const size_t desiredHotspotCount =
		std::min<size_t>(std::max<uint32_t>(1, options.HotspotCount), points.size());

	auto isSamePoint = [](const NormalizedPoint& left,
						  const NormalizedPoint& right) -> bool
	{
		const double dx = left.x - right.x;
		const double dy = left.y - right.y;
		return ((dx * dx) + (dy * dy)) <= 1e-8;
	};

	auto nearestPointTo = [&](const double targetX,
							  const double targetY) -> NormalizedPoint
	{
		size_t nearestIndex = 0;
		double nearestDistanceSq = std::numeric_limits<double>::max();
		for (size_t i = 0; i < points.size(); ++i)
		{
			const double dx = points[i].x - targetX;
			const double dy = points[i].y - targetY;
			const double distSq = (dx * dx) + (dy * dy);
			if (distSq < nearestDistanceSq)
			{
				nearestDistanceSq = distSq;
				nearestIndex = i;
			}
		}
		return points[nearestIndex];
	};

	std::vector<NormalizedPoint> anchors;
	anchors.reserve(desiredHotspotCount);
	for (const PeakCandidate& peak : peaks)
	{
		const NormalizedPoint candidate = nearestPointTo(peak.x, peak.y);
		if (std::none_of(anchors.begin(), anchors.end(),
						 [&](const NormalizedPoint& existing)
						 { return isSamePoint(existing, candidate); }))
		{
			anchors.push_back(candidate);
		}
		if (anchors.size() >= desiredHotspotCount)
		{
			break;
		}
	}

	if (anchors.empty())
	{
		anchors.push_back(points.front());
	}

	while (anchors.size() < desiredHotspotCount)
	{
		size_t bestIndex = points.size();
		double bestDistanceSq = -1.0;
		for (size_t i = 0; i < points.size(); ++i)
		{
			if (std::any_of(anchors.begin(), anchors.end(),
							[&](const NormalizedPoint& existing)
							{ return isSamePoint(existing, points[i]); }))
			{
				continue;
			}

			double nearestDistanceSq = std::numeric_limits<double>::max();
			for (const NormalizedPoint& anchor : anchors)
			{
				const double dx = points[i].x - anchor.x;
				const double dy = points[i].y - anchor.y;
				nearestDistanceSq = std::min(
					nearestDistanceSq, (dx * dx) + (dy * dy));
			}

			if (nearestDistanceSq > bestDistanceSq)
			{
				bestDistanceSq = nearestDistanceSq;
				bestIndex = i;
			}
		}

		if (bestIndex >= points.size())
		{
			break;
		}
		anchors.push_back(points[bestIndex]);
	}

	struct Aggregate
	{
		double sumX = 0.0;
		double sumY = 0.0;
		size_t count = 0;
		std::vector<NormalizedPoint> assigned;
	};

	std::vector<Aggregate> aggregates(anchors.size());
	for (const auto& point : points)
	{
		size_t nearestIndex = 0;
		double nearestDistanceSq = std::numeric_limits<double>::max();
		for (size_t i = 0; i < anchors.size(); ++i)
		{
			const double dx = point.x - anchors[i].x;
			const double dy = point.y - anchors[i].y;
			const double distSq = (dx * dx) + (dy * dy);
			if (distSq < nearestDistanceSq)
			{
				nearestDistanceSq = distSq;
				nearestIndex = i;
			}
		}

		Aggregate& aggregate = aggregates[nearestIndex];
		aggregate.sumX += point.x;
		aggregate.sumY += point.y;
		aggregate.count += 1;
		aggregate.assigned.push_back(point);
	}

	std::vector<HotspotVoronoiSample> hotspots;
	hotspots.reserve(aggregates.size());

	const uint32_t gridResolution = std::max<uint32_t>(8, options.DensityGridResolution);
	const double minRadius = 1.0 / static_cast<double>(gridResolution);
	const double gridCellDiagonal =
		std::sqrt(2.0) / static_cast<double>(gridResolution);

	for (const Aggregate& aggregate : aggregates)
	{
		if (aggregate.count == 0)
		{
			continue;
		}

		const double centerX =
			aggregate.sumX / static_cast<double>(aggregate.count);
		const double centerY =
			aggregate.sumY / static_cast<double>(aggregate.count);

		double maxDistance = 0.0;
		for (const auto& point : aggregate.assigned)
		{
			const double dx = point.x - centerX;
			const double dy = point.y - centerY;
			maxDistance = std::max(
				maxDistance, std::sqrt((dx * dx) + (dy * dy)));
		}

		hotspots.push_back({
			.x = Clamp01(centerX),
			.y = Clamp01(centerY),
			.weight = static_cast<double>(aggregate.count) /
					  static_cast<double>(points.size()),
			.radius = std::clamp(
				maxDistance + (gridCellDiagonal * 0.5), minRadius, 0.5),
		});
	}

	std::sort(hotspots.begin(), hotspots.end(),
			  [](const HotspotVoronoiSample& left, const HotspotVoronoiSample& right)
			  { return left.weight > right.weight; });

	return hotspots;
}

[[nodiscard]] glm::vec2 ToWorldPoint(
	const HotspotVoronoiSample& hotspot,
	const HotspotVoronoiHeuristic::Options& options)
{
	return {
		DenormalizeCoordinate(hotspot.x, -options.NetHalfExtent.x, options.NetHalfExtent.x),
		DenormalizeCoordinate(hotspot.y, -options.NetHalfExtent.y, options.NetHalfExtent.y),
	};
}

[[nodiscard]] glm::vec2 ClampToWorld(
	const glm::vec2& point, const HotspotVoronoiHeuristic::Options& options)
{
	return {
		std::clamp(point.x, -options.NetHalfExtent.x, options.NetHalfExtent.x),
		std::clamp(point.y, -options.NetHalfExtent.y, options.NetHalfExtent.y),
	};
}

[[nodiscard]] std::vector<glm::vec2> BuildFallbackSeeds(
	const uint32_t seedCount, const HotspotVoronoiHeuristic::Options& options)
{
	std::vector<glm::vec2> seeds;
	seeds.reserve(seedCount);
	if (seedCount == 0)
	{
		return seeds;
	}
	if (seedCount == 1)
	{
		seeds.emplace_back(0.0f, 0.0f);
		return seeds;
	}

	const float radiusX = options.NetHalfExtent.x * 0.62f;
	const float radiusY = options.NetHalfExtent.y * 0.62f;
	for (uint32_t i = 0; i < seedCount; ++i)
	{
		const double angle = (kTau * static_cast<double>(i)) /
							 static_cast<double>(seedCount);
		seeds.emplace_back(
			static_cast<float>(std::cos(angle)) * radiusX,
			static_cast<float>(std::sin(angle)) * radiusY);
	}
	return seeds;
}

[[nodiscard]] std::vector<size_t> SelectDistinctHotspots(
	const std::vector<HotspotVoronoiSample>& hotspots,
	const uint32_t count)
{
	std::vector<size_t> selected;
	if (hotspots.empty() || count == 0)
	{
		return selected;
	}

	selected.reserve(std::min<size_t>(hotspots.size(), count));
	selected.push_back(0);

	while (selected.size() < hotspots.size() && selected.size() < count)
	{
		size_t bestIndex = hotspots.size();
		double bestScore = -1.0;

		for (size_t candidateIndex = 0; candidateIndex < hotspots.size();
			 ++candidateIndex)
		{
			if (std::find(selected.begin(), selected.end(), candidateIndex) !=
				selected.end())
			{
				continue;
			}

			double nearestDistanceSq = std::numeric_limits<double>::max();
			for (const size_t selectedIndex : selected)
			{
				const double dx =
					hotspots[candidateIndex].x - hotspots[selectedIndex].x;
				const double dy =
					hotspots[candidateIndex].y - hotspots[selectedIndex].y;
				nearestDistanceSq = std::min(
					nearestDistanceSq, (dx * dx) + (dy * dy));
			}

			const double effectiveWeight =
				hotspots[candidateIndex].weight *
				(1.0 + (hotspots[candidateIndex].radius * 0.5));
			const double score =
				effectiveWeight * (nearestDistanceSq + 0.05);

			if (score > bestScore)
			{
				bestScore = score;
				bestIndex = candidateIndex;
			}
		}

		if (bestIndex >= hotspots.size())
		{
			break;
		}
		selected.push_back(bestIndex);
	}

	return selected;
}

[[nodiscard]] std::vector<uint32_t> AllocateSeedCounts(
	const std::vector<HotspotVoronoiSample>& hotspots, const uint32_t seedCount)
{
	std::vector<uint32_t> allocations(hotspots.size(), 0);
	if (hotspots.empty() || seedCount == 0)
	{
		return allocations;
	}

	const uint32_t distinctTarget =
		std::min<uint32_t>(seedCount, static_cast<uint32_t>(hotspots.size()));
	const std::vector<size_t> selected =
		SelectDistinctHotspots(hotspots, distinctTarget);
	for (const size_t index : selected)
	{
		allocations[index] = 1;
	}

	uint32_t remaining = seedCount - static_cast<uint32_t>(selected.size());
	if (remaining == 0 || selected.empty())
	{
		return allocations;
	}

	std::vector<double> exactExtras(hotspots.size(), 0.0);
	double totalWeight = 0.0;
	for (const size_t index : selected)
	{
		totalWeight += hotspots[index].weight *
					   (1.0 + (hotspots[index].radius * 0.75));
	}
	if (totalWeight <= 0.0)
	{
		totalWeight = static_cast<double>(selected.size());
	}

	for (const size_t index : selected)
	{
		const double effectiveWeight =
			hotspots[index].weight * (1.0 + (hotspots[index].radius * 0.75));
		exactExtras[index] =
			(static_cast<double>(remaining) * effectiveWeight) / totalWeight;
	}

	uint32_t assignedExtras = 0;
	for (const size_t index : selected)
	{
		const uint32_t whole =
			static_cast<uint32_t>(std::floor(exactExtras[index]));
		allocations[index] += whole;
		assignedExtras += whole;
	}

	while (assignedExtras < remaining)
	{
		size_t bestIndex = selected.front();
		double bestFraction = -1.0;
		for (const size_t index : selected)
		{
			const double fraction =
				exactExtras[index] - std::floor(exactExtras[index]);
			if (fraction > bestFraction)
			{
				bestFraction = fraction;
				bestIndex = index;
			}
		}
		allocations[bestIndex] += 1;
		exactExtras[bestIndex] = std::floor(exactExtras[bestIndex]);
		++assignedExtras;
	}

	return allocations;
}

void AppendSeedsForHotspot(
	std::vector<glm::vec2>& seeds,
	const HotspotVoronoiSample& hotspot,
	const uint32_t seedCountForHotspot,
	const size_t hotspotIndex,
	const HotspotVoronoiHeuristic::Options& options)
{
	if (seedCountForHotspot == 0)
	{
		return;
	}

	const glm::vec2 center = ToWorldPoint(hotspot, options);
	seeds.push_back(center);
	if (seedCountForHotspot == 1)
	{
		return;
	}

	const float minExtent =
		std::min(options.NetHalfExtent.x, options.NetHalfExtent.y);
	const float spread = std::clamp(
		static_cast<float>(hotspot.radius) * minExtent * 1.7f,
		6.0f,
		minExtent * 0.72f);
	const uint32_t ringCount = seedCountForHotspot - 1;
	const double baseAngle = static_cast<double>(hotspotIndex) * 2.39996322972865332;

	for (uint32_t ringIndex = 0; ringIndex < ringCount; ++ringIndex)
	{
		const double angle =
			baseAngle +
			(kTau * static_cast<double>(ringIndex)) /
				static_cast<double>(std::max<uint32_t>(1, ringCount));
		const float scale = ringCount == 1 ? 0.65f : (ringIndex % 2 == 0 ? 0.72f : 1.0f);
		const glm::vec2 offset(
			static_cast<float>(std::cos(angle)) * spread * scale,
			static_cast<float>(std::sin(angle)) * spread * scale);
		seeds.push_back(ClampToWorld(center + offset, options));
	}
}

[[nodiscard]] std::vector<glm::vec2> GenerateSeedPlacements(
	const std::vector<HotspotVoronoiSample>& hotspots,
	const uint32_t seedCount,
	const HotspotVoronoiHeuristic::Options& options)
{
	if (seedCount == 0)
	{
		return {};
	}
	if (hotspots.empty())
	{
		return BuildFallbackSeeds(seedCount, options);
	}

	std::vector<uint32_t> allocations = AllocateSeedCounts(hotspots, seedCount);
	std::vector<glm::vec2> seeds;
	seeds.reserve(seedCount);

	for (size_t hotspotIndex = 0; hotspotIndex < hotspots.size(); ++hotspotIndex)
	{
		AppendSeedsForHotspot(
			seeds, hotspots[hotspotIndex], allocations[hotspotIndex], hotspotIndex, options);
	}

	if (seeds.size() < seedCount)
	{
		std::vector<glm::vec2> fallback =
			BuildFallbackSeeds(seedCount - static_cast<uint32_t>(seeds.size()), options);
		seeds.insert(seeds.end(), fallback.begin(), fallback.end());
	}
	else if (seeds.size() > seedCount)
	{
		seeds.resize(seedCount);
	}

	return seeds;
}

[[nodiscard]] std::vector<glm::vec2> ClipPolygon(
	const std::vector<glm::vec2>& polygon, const glm::vec2& normal, const float c)
{
	if (polygon.empty())
	{
		return {};
	}

	auto inside = [&](const glm::vec2& point) -> bool
	{
		return glm::dot(point, normal) <= c + 1e-5f;
	};

	auto intersect = [&](const glm::vec2& a, const glm::vec2& b) -> glm::vec2
	{
		const glm::vec2 ab = b - a;
		const float denom = glm::dot(ab, normal);
		if (std::abs(denom) < 1e-8f)
		{
			return a;
		}
		const float t = (c - glm::dot(a, normal)) / denom;
		return a + glm::clamp(t, 0.0f, 1.0f) * ab;
	};

	std::vector<glm::vec2> out;
	out.reserve(polygon.size() + 4);
	for (size_t i = 0; i < polygon.size(); ++i)
	{
		const glm::vec2 current = polygon[i];
		const glm::vec2 previous = polygon[(i + polygon.size() - 1) % polygon.size()];
		const bool currentInside = inside(current);
		const bool previousInside = inside(previous);

		if (currentInside)
		{
			if (!previousInside)
			{
				out.push_back(intersect(previous, current));
			}
			out.push_back(current);
		}
		else if (previousInside)
		{
			out.push_back(intersect(previous, current));
		}
	}
	return out;
}
}  // namespace

HotspotVoronoiHeuristic::HotspotVoronoiHeuristic() = default;

void HotspotVoronoiHeuristic::SetAvailableServerCount(uint32_t count)
{
	requestedServerCount = count;
}

void HotspotVoronoiHeuristic::SetHotspotCount(uint32_t count)
{
	options.HotspotCount = std::max<uint32_t>(1, count);
}

std::vector<HotspotVoronoiSample> HotspotVoronoiHeuristic::BuildHotspotsFromEntities(
	const std::span<const AtlasTransform>& span, const Options& options)
{
	const std::vector<NormalizedPoint> points = NormalizeTransforms(span, options);
	return BuildHotspots(points, options);
}

std::vector<glm::vec2> HotspotVoronoiHeuristic::GenerateAlgorithmicSeeds(
	const std::vector<HotspotVoronoiSample>& hotspots,
	const uint32_t serverCount,
	const Options& options)
{
	return GenerateSeedPlacements(
		hotspots, std::max<uint32_t>(1, serverCount), options);
}

std::vector<VoronoiBounds> HotspotVoronoiHeuristic::BuildCellsFromSeeds(
	const std::vector<glm::vec2>& seeds, const Options& options)
{
	std::vector<VoronoiBounds> cells;
	cells.reserve(seeds.size());

	for (uint32_t i = 0; i < seeds.size(); ++i)
	{
		const glm::vec2 seed = seeds[i];
		VoronoiBounds bound;
		bound.ID = static_cast<BoundsID>(i);
		bound.site = seed;
		bound.halfPlanes.clear();
		bound.halfPlanes.reserve(seeds.size() > 0 ? seeds.size() - 1 : 0);
		bound.vertices.clear();
		for (uint32_t j = 0; j < seeds.size(); ++j)
		{
			if (j == i)
			{
				continue;
			}

			const glm::vec2 otherSeed = seeds[j];
			const glm::vec2 normal = otherSeed - seed;
			const float c = (glm::dot(otherSeed, otherSeed) - glm::dot(seed, seed)) * 0.5f;
			VoronoiHalfPlane plane;
			plane.normal = normal;
			plane.c = c;
			bound.halfPlanes.push_back(plane);
		}
		cells.push_back(std::move(bound));
	}

	return cells;
}

uint32_t HotspotVoronoiHeuristic::GetActiveServerCount() const
{
	return activeServerCount;
}

uint32_t HotspotVoronoiHeuristic::GetHotspotCount() const
{
	return options.HotspotCount;
}

const std::vector<HotspotVoronoiSample>& HotspotVoronoiHeuristic::GetHotspots() const
{
	return hotspots;
}

const std::vector<glm::vec2>& HotspotVoronoiHeuristic::GetSeeds() const
{
	return seeds;
}

void HotspotVoronoiHeuristic::Compute(const std::span<const AtlasTransform>& span)
{
	activeServerCount =
		requestedServerCount > 0 ? requestedServerCount : options.DefaultServerCount;
	activeServerCount = std::max<uint32_t>(1, activeServerCount);

	hotspots = BuildHotspotsFromEntities(span, options);
	seeds = GenerateAlgorithmicSeeds(hotspots, activeServerCount, options);
	cells = BuildCellsFromSeeds(seeds, options);

	logger.DebugFormatted(
		"HotspotVoronoiHeuristic::Compute: entities={} hotspots={} seeds={} bounds={}",
		span.size(), hotspots.size(), seeds.size(), cells.size());
}

uint32_t HotspotVoronoiHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(cells.size());
}

void HotspotVoronoiHeuristic::GetBounds(std::vector<VoronoiBounds>& out_bounds) const
{
	out_bounds = cells;
}

void HotspotVoronoiHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<VoronoiBounds>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type HotspotVoronoiHeuristic::GetType() const
{
	return IHeuristic::Type::eHotspotVoronoi;
}

void HotspotVoronoiHeuristic::SerializeBounds(
	std::unordered_map<BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const VoronoiBounds& cell : cells)
	{
		auto [it, inserted] = bws.emplace(cell.ID, ByteWriter{});
		(void)inserted;
		cell.Serialize(it->second);
	}
}

void HotspotVoronoiHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(cells.size());
	for (const auto& cell : cells)
	{
		cell.Serialize(bw);
	}
}

void HotspotVoronoiHeuristic::Deserialize(ByteReader& br)
{
	const size_t cellCount = br.u64();
	cells.resize(cellCount);
	hotspots.clear();
	seeds.clear();
	for (size_t i = 0; i < cells.size(); ++i)
	{
		cells[i].Deserialize(br);
	}
}

std::optional<BoundsID> HotspotVoronoiHeuristic::QueryPosition(vec3 p) const
{
	for (const auto& cell : cells)
	{
		if (cell.Contains(p))
		{
			return cell.ID;
		}
	}
	return std::nullopt;
}

const IBounds& HotspotVoronoiHeuristic::GetBound(BoundsID id) const
{
	for (const auto& cell : cells)
	{
		if (cell.ID == id)
		{
			return cell;
		}
	}
	throw std::runtime_error("Invalid ID");
}

#include "VoronoiHeuristic.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/IBounds.hpp"

namespace
{
[[nodiscard]] std::vector<glm::vec2> ClipPolygon(
	const std::vector<glm::vec2>& polygon, const glm::vec2& normal, const float c)
{
	if (polygon.empty())
	{
		return {};
	}

	auto isInside = [&](const glm::vec2& point)
	{
		return glm::dot(point, normal) <= c + 1e-5f;
	};

	auto intersect = [&](const glm::vec2& start, const glm::vec2& end)
	{
		const glm::vec2 delta = end - start;
		const float denom = glm::dot(normal, delta);
		if (std::abs(denom) < 1e-8f)
		{
			return start;
		}
		const float t = (c - glm::dot(normal, start)) / denom;
		return start + delta * std::clamp(t, 0.0f, 1.0f);
	};

	std::vector<glm::vec2> out;
	out.reserve(polygon.size() + 1);
	for (size_t i = 0; i < polygon.size(); ++i)
	{
		const glm::vec2 current = polygon[i];
		const glm::vec2 previous = polygon[(i + polygon.size() - 1) % polygon.size()];
		const bool currentInside = isInside(current);
		const bool previousInside = isInside(previous);
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

VoronoiHeuristic::VoronoiHeuristic() = default;

void VoronoiHeuristic::SetSeedCount(uint32_t count)
{
	if (count == 0)
	{
		count = 1;
	}
	options.SeedCount = count;
}

const std::vector<glm::vec2>& VoronoiHeuristic::GetSeeds() const
{
	return _seeds;
}

void VoronoiHeuristic::Compute(const std::span<const AtlasTransform>& span)
{
	(void)span;

	// Build canonical Voronoi cells as site + half-plane constraints. Cells are
	// intentionally left unbounded here; renderers clip them to a viewport.

	const uint32_t seedCount = std::max<uint32_t>(1, options.SeedCount);

	const float minX = -options.NetHalfExtent.x;
	const float maxX = options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;
	const float maxY = options.NetHalfExtent.y;

	// For debugging we retain the old behaviour where periodic recomputes can
	// visibly reshuffle the heuristic, so each compute revision perturbs the RNG
	// seed. Once hotspot-driven seeding lands, this can be replaced by data-
	// driven seeds instead of revision-driven movement.
	const uint32_t revisionSeed =
		static_cast<uint32_t>((_computeRevision * 0x85EBCA6Bu) & 0xFFFFFFFFu);
	++_computeRevision;
	std::mt19937 rng(0x9E3779B9u ^ seedCount ^ revisionSeed);
	std::uniform_real_distribution<float> distX(minX, maxX);
	std::uniform_real_distribution<float> distY(minY, maxY);

	_seeds.clear();
	_seeds.reserve(seedCount);
	for (uint32_t i = 0; i < seedCount; ++i)
	{
		_seeds.emplace_back(distX(rng), distY(rng));
	}

	_cells.clear();
	_cells.reserve(seedCount);

	for (uint32_t i = 0; i < seedCount; ++i)
	{
		const glm::vec2 si = _seeds[i];
		VoronoiBounds bound;
		bound.ID = static_cast<BoundsID>(i);
		bound.site = si;
		bound.halfPlanes.clear();
		bound.vertices.clear();
		std::vector<VoronoiHalfPlane> clipPlanes;
		clipPlanes.reserve(seedCount > 0 ? seedCount - 1 : 0);

		for (uint32_t j = 0; j < seedCount; ++j)
		{
			if (j == i)
			{
				continue;
			}
			const glm::vec2 sj = _seeds[j];

			VoronoiHalfPlane plane;
			plane.normal = sj - si;
			plane.c = (glm::dot(sj, sj) - glm::dot(si, si)) * 0.5f;
			clipPlanes.push_back(plane);
		}

		std::vector<glm::vec2> polygon = {
			{minX, minY},
			{maxX, minY},
			{maxX, maxY},
			{minX, maxY},
		};
		for (const VoronoiHalfPlane& plane : clipPlanes)
		{
			polygon = ClipPolygon(polygon, plane.normal, plane.c);
			if (polygon.size() < 3)
			{
				break;
			}
		}
		bound.vertices = std::move(polygon);

		_cells.push_back(std::move(bound));
	}

	logger.DebugFormatted("VoronoiHeuristic::Compute: seed_count={} bounds={} revision={}",
						  seedCount, _cells.size(), _computeRevision);
}

uint32_t VoronoiHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(_cells.size());
}

void VoronoiHeuristic::GetBounds(std::vector<VoronoiBounds>& out_bounds) const
{
	out_bounds = _cells;
}

void VoronoiHeuristic::GetBoundDeltas(std::vector<TBoundDelta<VoronoiBounds>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type VoronoiHeuristic::GetType() const
{
	return IHeuristic::Type::eVoronoi;
}

void VoronoiHeuristic::SerializeBounds(std::unordered_map<BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const VoronoiBounds& cell : _cells)
	{
		auto [it, inserted] = bws.emplace(cell.ID, ByteWriter{});
		(void)inserted;
		cell.Serialize(it->second);
	}
}

void VoronoiHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(_cells.size());
	for (const auto& cell : _cells)
	{
		cell.Serialize(bw);
	}
}

void VoronoiHeuristic::Deserialize(ByteReader& br)
{
	const size_t cellCount = br.u64();
	_cells.resize(cellCount);
	_seeds.clear();
	_seeds.reserve(cellCount);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		_cells[i].Deserialize(br);
		_seeds.push_back(_cells[i].site);
	}
}

std::optional<BoundsID> VoronoiHeuristic::QueryPosition(vec3 p) const
{
	for (const auto& cell : _cells)
	{
		if (cell.Contains(p))
		{
			return cell.ID;
		}
	}
	return std::nullopt;
}

const IBounds& VoronoiHeuristic::GetBound(BoundsID id) const
{
	for (const auto& cell : _cells)
	{
		if (cell.ID == id)
		{
			return cell;
		}
	}
	throw std::runtime_error("Invalid ID");
}

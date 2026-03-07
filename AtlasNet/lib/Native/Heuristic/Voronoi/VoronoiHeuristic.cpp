#include "VoronoiHeuristic.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/IBounds.hpp"

VoronoiHeuristic::VoronoiHeuristic() = default;

void VoronoiHeuristic::SetSeedCount(uint32_t count)
{
	if (count == 0)
	{
		count = 1;
	}
	options.SeedCount = count;
}

void VoronoiHeuristic::Compute(const std::span<const Transform>& span)
{
	// Build Voronoi cells by clipping a bounding box polygon with the
	// perpendicular bisector half-planes of all other seeds.

	const uint32_t seedCount = 5;  // std::max<uint32_t>(1, options.SeedCount);

	const float minX = -options.NetHalfExtent.x;
	const float maxX = options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;
	const float maxY = options.NetHalfExtent.y;

	// Deterministic RNG so WatchDog recompute doesn't reshuffle regions.
	static std::mt19937 rng(0x9E3779B9u ^ seedCount);
	static std::uniform_real_distribution<float> distX(minX, maxX);
	static std::uniform_real_distribution<float> distY(minY, maxY);

	_seeds.clear();
	_seeds.reserve(seedCount);
	for (uint32_t i = 0; i < seedCount; ++i)
	{
		_seeds.emplace_back(distX(rng), distY(rng));
	}

	auto clipPolygon = [](const std::vector<glm::vec2>& poly, const glm::vec2& n,
						  const float c) -> std::vector<glm::vec2>
	{
		if (poly.empty())
		{
			return {};
		}

		auto inside = [&](const glm::vec2& p) -> bool { return glm::dot(p, n) <= c + 1e-5f; };

		auto intersect = [&](const glm::vec2& a, const glm::vec2& b) -> glm::vec2
		{
			const glm::vec2 ab = b - a;
			const float denom = glm::dot(ab, n);
			if (std::abs(denom) < 1e-8f)
			{
				return a;
			}
			const float t = (c - glm::dot(a, n)) / denom;
			return a + glm::clamp(t, 0.0f, 1.0f) * ab;
		};

		std::vector<glm::vec2> out;
		out.reserve(poly.size() + 4);
		for (size_t i = 0; i < poly.size(); ++i)
		{
			const glm::vec2 curr = poly[i];
			const glm::vec2 prev = poly[(i + poly.size() - 1) % poly.size()];
			const bool currIn = inside(curr);
			const bool prevIn = inside(prev);

			if (currIn)
			{
				if (!prevIn)
				{
					out.push_back(intersect(prev, curr));
				}
				out.push_back(curr);
			}
			else if (prevIn)
			{
				out.push_back(intersect(prev, curr));
			}
		}
		return out;
	};

	_cells.clear();
	_cells.reserve(seedCount);

	for (uint32_t i = 0; i < seedCount; ++i)
	{
		const glm::vec2 si = _seeds[i];
		std::vector<glm::vec2> poly = {
			{minX, minY},
			{maxX, minY},
			{maxX, maxY},
			{minX, maxY},
		};

		for (uint32_t j = 0; j < seedCount; ++j)
		{
			if (j == i)
			{
				continue;
			}
			const glm::vec2 sj = _seeds[j];
			const glm::vec2 n = sj - si;
			const float c = (glm::dot(sj, sj) - glm::dot(si, si)) * 0.5f;
			poly = clipPolygon(poly, n, c);
			if (poly.size() < 3)
			{
				break;
			}
		}

		VoronoiBounds bound;
		bound.ID = static_cast<BoundsID>(i);
		bound.vertices = std::move(poly);
		_cells.push_back(std::move(bound));
	}

	logger.DebugFormatted("VoronoiHeuristic::Compute: seed_count={} bounds={}", seedCount,
						  _cells.size());
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
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		_cells[i].Deserialize(br);
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

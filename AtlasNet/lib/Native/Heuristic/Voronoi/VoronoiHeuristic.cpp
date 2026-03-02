#include "VoronoiHeuristic.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "Global/Serialize/ByteWriter.hpp"

VoronoiHeuristic::VoronoiHeuristic() = default;

void VoronoiHeuristic::SetTargetCellCount(uint32_t count)
{
	if (count == 0)
	{
		count = 1;
	}
	options.TargetCellCount = count;
}

void VoronoiHeuristic::Compute(const std::span<const AtlasEntityMinimal>&)
{
	// Build a random, but axis-aligned, tiling over the same net area
	// as the grid/quadtree heuristics. We generate random interior
	// cut positions along X and Y and use them to define non-uniform
	// stripes; their cartesian product yields a set of rectangular
	// GridShape bounds that fully cover the region.

	const uint32_t requestedCells = std::max<uint32_t>(1, options.TargetCellCount);
	const float rootWidthX = options.NetHalfExtent.x * 2.0f;
	const float rootWidthY = options.NetHalfExtent.y * 2.0f;

	// Choose an integer grid dimension N such that N^2 ~= requestedCells.
	const float sideF = std::sqrt(static_cast<float>(requestedCells));
	uint32_t side = static_cast<uint32_t>(std::round(sideF));
	if (side == 0)
	{
		side = 1;
	}
	const uint32_t actualCells = side * side;

	logger.DebugFormatted(
		"VoronoiHeuristic::Compute: requested_cells={} -> grid={}x{} ({} cells)",
		requestedCells, side, side, actualCells);

	const float minX = -options.NetHalfExtent.x;
	const float maxX = options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;
	const float maxY = options.NetHalfExtent.y;

	// Random stripe boundaries along X and Y.
	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> distX(minX, maxX);
	std::uniform_real_distribution<float> distY(minY, maxY);

	std::vector<float> xCuts;
	std::vector<float> yCuts;
	xCuts.reserve(side + 1);
	yCuts.reserve(side + 1);

	xCuts.push_back(minX);
	yCuts.push_back(minY);
	for (uint32_t i = 1; i < side; ++i)
	{
		xCuts.push_back(distX(rng));
		yCuts.push_back(distY(rng));
	}
	xCuts.push_back(maxX);
	yCuts.push_back(maxY);

	std::sort(xCuts.begin(), xCuts.end());
	std::sort(yCuts.begin(), yCuts.end());

	_cells.clear();
	_cells.resize(actualCells);

	IBounds::BoundsID nextId = 0;
	for (uint32_t row = 0; row < side; ++row)
	{
		const float stripeYMin = yCuts[row];
		const float stripeYMax = yCuts[row + 1];
		const float centerY = 0.5f * (stripeYMin + stripeYMax);
		const float halfHeight = 0.5f * std::max(stripeYMax - stripeYMin, 0.001f);

		for (uint32_t col = 0; col < side; ++col)
		{
			const float stripeXMin = xCuts[col];
			const float stripeXMax = xCuts[col + 1];
			const float centerX = 0.5f * (stripeXMin + stripeXMax);
			const float halfWidth = 0.5f * std::max(stripeXMax - stripeXMin, 0.001f);

			GridShape cell;
			cell.ID = nextId++;
			cell.aabb.SetCenterExtents(
				vec3(centerX, centerY, 0.0f),
				vec3(halfWidth, halfHeight, 5.0f));

			const size_t index = static_cast<size_t>(row * side + col);
			_cells[index] = cell;
		}
	}
}

uint32_t VoronoiHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(_cells.size());
}

void VoronoiHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const
{
	out_bounds = _cells;
}

void VoronoiHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<GridShape>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type VoronoiHeuristic::GetType() const
{
	return IHeuristic::Type::eVoronoi;
}

void VoronoiHeuristic::SerializeBounds(
	std::unordered_map<IBounds::BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const GridShape& cell : _cells)
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
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		_cells[i].Deserialize(br);
	}
}

std::optional<IBounds::BoundsID> VoronoiHeuristic::QueryPosition(vec3 p)
{
	for (const auto& cell : _cells)
	{
		if (cell.aabb.contains(p))
		{
			return cell.ID;
		}
	}
	return std::nullopt;
}

std::unique_ptr<IBounds> VoronoiHeuristic::GetBound(IBounds::BoundsID id)
{
	for (const auto& cell : _cells)
	{
		if (cell.ID == id)
		{
			return std::make_unique<GridShape>(cell);
		}
	}
	return nullptr;
}


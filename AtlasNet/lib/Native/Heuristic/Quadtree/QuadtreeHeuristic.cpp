#include "QuadtreeHeuristic.hpp"

#include <cmath>
#include <stdexcept>

#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/IBounds.hpp"

QuadtreeHeuristic::QuadtreeHeuristic() = default;

void QuadtreeHeuristic::SetTargetLeafCount(uint32_t count)
{
	if (count == 0)
	{
		count = 1;
	}
	options.TargetLeafCount = count;
}

void QuadtreeHeuristic::Compute(const std::span<const AtlasTransform>& span)
{
	// Quadtree-style subdivision: start from a single root region covering the
	// legacy grid area and repeatedly subdivide nodes into 4 children until we
	// reach a leaf count >= requested. We round the requested leaf count up to
	// the nearest value of the form (1 + 3*k), since each subdivision replaces
	// 1 node with 4 children (+3 leaves).

	const uint32_t requestedLeaves = std::max<uint32_t>(1, options.TargetLeafCount);

	uint32_t targetLeaves = requestedLeaves;
	if (targetLeaves == 1)
	{
		// Degenerate case: a single region equal to the whole area.
		targetLeaves = 1;
	}
	else
	{
		// Ensure targetLeaves = 1 (mod 3) so it is reachable by repeated splits.
		while ((targetLeaves - 1U) % 3U != 0U)
		{
			++targetLeaves;
		}
	}

	const float minX = -options.NetHalfExtent.x;
	const float maxX = options.NetHalfExtent.x;
	const float minY = -options.NetHalfExtent.y;
	const float maxY = options.NetHalfExtent.y;

	struct Node
	{
		glm::vec2 min;
		glm::vec2 max;
	};

	std::vector<Node> leaves;
	leaves.reserve(targetLeaves);
	leaves.push_back(Node{glm::vec2(minX, minY), glm::vec2(maxX, maxY)});

	while (leaves.size() < targetLeaves)
	{
		// Always split the last leaf; the exact ordering does not matter for
		// correctness, but this keeps behaviour deterministic.
		const Node node = leaves.back();
		leaves.pop_back();

		const float midX = 0.5f * (node.min.x + node.max.x);
		const float midY = 0.5f * (node.min.y + node.max.y);

		// Bottom-left
		leaves.push_back(
			Node{glm::vec2(node.min.x, node.min.y), glm::vec2(midX, midY)});
		// Bottom-right
		leaves.push_back(
			Node{glm::vec2(midX, node.min.y), glm::vec2(node.max.x, midY)});
		// Top-left
		leaves.push_back(
			Node{glm::vec2(node.min.x, midY), glm::vec2(midX, node.max.y)});
		// Top-right
		leaves.push_back(
			Node{glm::vec2(midX, midY), glm::vec2(node.max.x, node.max.y)});
	}

	logger.DebugFormatted(
		"QuadtreeHeuristic::Compute: requested_leaves={} -> target_leaves={} "
		"({} nodes)",
		requestedLeaves, targetLeaves, leaves.size());

	_cells.clear();
	_cells.resize(leaves.size());

	BoundsID nextId = 0;
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		const Node& n = leaves[i];
		const float centerX = 0.5f * (n.min.x + n.max.x);
		const float centerY = 0.5f * (n.min.y + n.max.y);
		const float halfWidth = 0.5f * std::max(n.max.x - n.min.x, 0.001f);
		const float halfHeight = 0.5f * std::max(n.max.y - n.min.y, 0.001f);

		GridShape cell;
		cell.ID = nextId++;
		cell.aabb.SetCenterExtents(
			vec3(centerX, centerY, 0.0f),
			vec3(halfWidth, halfHeight, 5.0f));

		_cells[i] = cell;
	}
}

uint32_t QuadtreeHeuristic::GetBoundsCount() const
{
	return static_cast<uint32_t>(_cells.size());
}

void QuadtreeHeuristic::GetBounds(std::vector<GridShape>& out_bounds) const
{
	out_bounds = _cells;
}

void QuadtreeHeuristic::GetBoundDeltas(
	std::vector<TBoundDelta<GridShape>>& out_deltas) const
{
	out_deltas.clear();
}

IHeuristic::Type QuadtreeHeuristic::GetType() const
{
	return IHeuristic::Type::eQuadtree;
}

void QuadtreeHeuristic::SerializeBounds(
	std::unordered_map<BoundsID, ByteWriter>& bws)
{
	bws.clear();
	for (const GridShape& cell : _cells)
	{
		auto [it, inserted] = bws.emplace(cell.ID, ByteWriter{});
		(void)inserted;
		cell.Serialize(it->second);
	}
}

void QuadtreeHeuristic::Serialize(ByteWriter& bw) const
{
	bw.u64(_cells.size());
	for (const auto& cell : _cells)
	{
		cell.Serialize(bw);
	}
}

void QuadtreeHeuristic::Deserialize(ByteReader& br)
{
	const size_t cellCount = br.u64();
	_cells.resize(cellCount);
	for (size_t i = 0; i < _cells.size(); ++i)
	{
		_cells[i].Deserialize(br);
	}
}

std::optional<BoundsID> QuadtreeHeuristic::QueryPosition(vec3 p) const
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

const IBounds& QuadtreeHeuristic::GetBound(BoundsID id) const
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


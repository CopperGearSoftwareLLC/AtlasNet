#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"

// Polygonal bounds for Voronoi-style partitioning.
// Stored as a 2D polygon in XY; Z is ignored for Contains tests.
struct VoronoiBounds : public IBounds
{
	std::vector<glm::vec2> vertices;

	bool Contains(vec3 p) const override
	{
		if (vertices.size() < 3)
		{
			return false;
		}

		// Ray casting in 2D.
		const float x = p.x;
		const float y = p.y;
		bool inside = false;
		for (size_t i = 0, j = vertices.size() - 1; i < vertices.size();
			 j = i++)
		{
			const glm::vec2 vi = vertices[i];
			const glm::vec2 vj = vertices[j];

			const bool intersects =
				((vi.y > y) != (vj.y > y)) &&
				(x < (vj.x - vi.x) * (y - vi.y) / ((vj.y - vi.y) + 1e-12f) + vi.x);
			if (intersects)
			{
				inside = !inside;
			}
		}
		return inside;
	}

	[[nodiscard]] vec3 GetCenter() const override
	{
		if (vertices.empty())
		{
			return vec3(0.0f);
		}
		if (vertices.size() < 3)
		{
			glm::vec2 avg(0.0f);
			for (const auto& v : vertices)
			{
				avg += v;
			}
			avg /= static_cast<float>(vertices.size());
			return vec3(avg.x, avg.y, 0.0f);
		}

		// Polygon centroid via shoelace formula.
		double area2 = 0.0;
		double cx = 0.0;
		double cy = 0.0;
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			const auto& a = vertices[i];
			const auto& b = vertices[(i + 1) % vertices.size()];
			const double cross = static_cast<double>(a.x) * static_cast<double>(b.y) -
								 static_cast<double>(b.x) * static_cast<double>(a.y);
			area2 += cross;
			cx += (static_cast<double>(a.x) + static_cast<double>(b.x)) * cross;
			cy += (static_cast<double>(a.y) + static_cast<double>(b.y)) * cross;
		}

		if (std::abs(area2) < 1e-10)
		{
			glm::vec2 avg(0.0f);
			for (const auto& v : vertices)
			{
				avg += v;
			}
			avg /= static_cast<float>(vertices.size());
			return vec3(avg.x, avg.y, 0.0f);
		}

		const double inv = 1.0 / (3.0 * area2);
		return vec3(static_cast<float>(cx * inv), static_cast<float>(cy * inv),
					0.0f);
	}

	std::string ToDebugString() const override
	{
		return std::format("VoronoiBounds(ID={}, vertices={})", ID, vertices.size());
	}

   protected:
	void Internal_SerializeData(ByteWriter& bw) const override
	{
		bw.u32(static_cast<uint32_t>(vertices.size()));
		for (const auto& v : vertices)
		{
			bw.f32(v.x).f32(v.y);
		}
	}

	void Internal_DeserializeData(ByteReader& br) override
	{
		const uint32_t count = br.u32();
		vertices.clear();
		vertices.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			const float x = br.f32();
			const float y = br.f32();
			vertices.emplace_back(x, y);
		}
	}
};


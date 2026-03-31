#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Global/pch.hpp"
#include "Heuristic/IBounds.hpp"

struct VoronoiHalfPlane
{
	glm::vec2 normal = glm::vec2(0.0f);
	float c = 0.0f;
};

// Canonical Voronoi bounds are stored as a site plus the half-plane constraints
// defining the cell. Optional vertices may be present for bounded/debug views.
struct VoronoiBounds : public IBounds
{
	glm::vec2 site = glm::vec2(0.0f);
	std::vector<VoronoiHalfPlane> halfPlanes;
	std::vector<glm::vec2> vertices;

	bool Contains(vec3 p) const override
	{
		if (!halfPlanes.empty())
		{
			const glm::vec2 point(p.x, p.y);
			for (const auto& plane : halfPlanes)
			{
				if (glm::dot(point, plane.normal) > plane.c + 1e-5f)
				{
					return false;
				}
			}
			return true;
		}

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
		if (!halfPlanes.empty())
		{
			return vec3(site.x, site.y, 0.0f);
		}
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
		return std::format("VoronoiBounds(ID={}, site=({}, {}), halfPlanes={}, vertices={})", ID,
						   site.x, site.y, halfPlanes.size(), vertices.size());
	}

   protected:
	void Internal_SerializeData(ByteWriter& bw) const override
	{
		static constexpr uint32_t kFormatVersion = 1;
		bw.u32(kFormatVersion);
		bw.f32(site.x).f32(site.y);
		bw.u32(static_cast<uint32_t>(halfPlanes.size()));
		for (const auto& plane : halfPlanes)
		{
			bw.f32(plane.normal.x).f32(plane.normal.y).f32(plane.c);
		}
		bw.u32(static_cast<uint32_t>(vertices.size()));
		for (const auto& v : vertices)
		{
			bw.f32(v.x).f32(v.y);
		}
	}

	void Internal_DeserializeData(ByteReader& br) override
	{
		const uint32_t versionOrVertexCount = br.u32();
		halfPlanes.clear();
		site = glm::vec2(0.0f);

		uint32_t count = 0;
		if (versionOrVertexCount <= 2)
		{
			const uint32_t version = versionOrVertexCount;
			if (version == 1)
			{
				site.x = br.f32();
				site.y = br.f32();

				const uint32_t halfPlaneCount = br.u32();
				halfPlanes.reserve(halfPlaneCount);
				for (uint32_t i = 0; i < halfPlaneCount; ++i)
				{
					VoronoiHalfPlane plane;
					plane.normal.x = br.f32();
					plane.normal.y = br.f32();
					plane.c = br.f32();
					halfPlanes.push_back(plane);
				}

				count = br.u32();
			}
			else
			{
				throw std::runtime_error("Unsupported VoronoiBounds format version");
			}
		}
		else
		{
			count = versionOrVertexCount;
		}

		vertices.clear();
		vertices.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			const float x = br.f32();
			const float y = br.f32();
			vertices.emplace_back(x, y);
		}

		if (halfPlanes.empty() && !vertices.empty())
		{
			site = glm::vec2(GetCenter().x, GetCenter().y);
		}
	}
};

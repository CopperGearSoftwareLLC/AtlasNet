#pragma once

#include "Entity.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "Types/AABB.hpp"
#include "pch.hpp"
template <typename T>
struct AtlasEntitySpan
{
    T* data = nullptr;
    size_t count = 0;

    // Extra info
    vec3 midpoint;
    AABB<3, float> bounds;

    AtlasEntitySpan() = default;
    AtlasEntitySpan(T* ptr, size_t cnt) : data(ptr), count(cnt) { computeBounds(); }

    T& operator[](size_t idx) { return data[idx]; }
    const T& operator[](size_t idx) const { return data[idx]; }

    T* begin() { return data; }
    T* end() { return data + count; }
    const T* begin() const { return data; }
    const T* end() const { return data + count; }

    void computeBounds()
    {
        if (!data || count == 0) return;
        bounds.min = data[0].transform.position;
        bounds.max = data[0].transform.position;
        vec3 sum = vec3(0.0f);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& pos = data[i].transform.position;
            sum += pos;
            bounds.min = glm::min(bounds.min, pos);
            bounds.max = glm::max(bounds.max, pos);
        }
        midpoint = sum / float(count);
    }

    // allow conversion to const span
    operator AtlasEntitySpan<const T>() const
    {
        AtlasEntitySpan<const T> cspan;
        cspan.data = data;
        cspan.count = count;
        cspan.midpoint = midpoint;
        cspan.bounds = bounds;
        return cspan;
    }
};
template <typename T = AtlasEntityMinimal>
struct AtlasEntityList
{
	std::vector<T> entities;

	// Extra info
	vec3 midpoint;
	AABB<3, float> bounds;

	AtlasEntitySpan<T> span() { return AtlasEntitySpan<T>(entities.data(), entities.size()); }

	void push_back(const T& e)
	{
		entities.push_back(e);
		recompute();
	}
	void emplace_back(T&& e)
	{
		entities.emplace_back(std::move(e));
		recompute();
	}

	void recompute()
	{
		if (entities.empty())
			return;
		bounds.min = entities[0].transform.position;
		bounds.max = entities[0].transform.position;
		vec3 sum = vec3(0.0f);
		for (auto& e : entities)
		{
			const auto& pos = e.transform.position;
			sum += pos;
			bounds.min = glm::min(bounds.min, pos);
			bounds.max = glm::max(bounds.max, pos);
		}
		midpoint = sum / float(entities.size());
	}

	void Serialize(ByteWriter& bw) const
	{
		bw.write_scalar<uint64_t>(entities.size());
		for (const auto& e : entities) e.Serialize(bw);
	}

	void Deserialize(ByteReader& br)
	{
		const size_t n = br.read_scalar<uint64_t>();
		entities.resize(n);
		for (size_t i = 0; i < n; ++i) entities[i].Deserialize(br);
		recompute();
	}
};

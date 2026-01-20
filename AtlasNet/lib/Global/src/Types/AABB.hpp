#pragma once

#include "AtlasObject.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#pragma once

#include <algorithm>
#include <glm/glm.hpp>
#include <limits>
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
template <uint8_t Dim, typename Type, glm::qualifier Q = glm::defaultp>
struct AABB : public AtlasObject
{
	static_assert(Dim > 0, "AABB dimension must be > 0");

	using value_type = Type;
	using vectype = glm::vec<Dim, Type, Q>;

	vectype min;
	vectype max;

	// -------------------------------------------------
	// Constructors
	// -------------------------------------------------

	// Invalid / empty box
	AABB()
		: min(vectype(std::numeric_limits<Type>::max())),
		  max(vectype(std::numeric_limits<Type>::lowest()))
	{
	}

	AABB(const vectype& min_, const vectype& max_) : min(min_), max(max_) {}

	// From center + half extents
	static AABB FromCenterExtents(const vectype& center, const vectype& halfExtents)
	{
		return AABB(center - halfExtents, center + halfExtents);
	}
	void SetCenterExtents(const vectype& center, const vectype& halfExtents)
	{
		*this = FromCenterExtents(center, halfExtents);
	}
	void Serialize(ByteWriter& bw) const override
	{
		bw.write_vector<Dim>(min);
		bw.write_vector<Dim>(max);
	}
	void Deserialize(ByteReader& br) override
	{
		min = br.read_vector<Dim, decltype(min)>();
		max = br.read_vector<Dim, decltype(min)>();
	}

	// -------------------------------------------------
	// Properties
	// -------------------------------------------------

	vectype center() const { return (min + max) * Type(0.5); }

	vectype size() const { return max - min; }

	vectype halfExtents() const { return (max - min) * Type(0.5); }

	Type volume() const
	{
		vectype s = size();
		Type v = Type(1);
		for (uint8_t i = 0; i < Dim; ++i) v *= s[i];
		return v;
	}

	bool valid() const
	{
		for (uint8_t i = 0; i < Dim; ++i)
		{
			if (min[i] > max[i])
				return false;
		}
		return true;
	}

	// -------------------------------------------------
	// Expansion
	// -------------------------------------------------

	void expand(const vectype& p)
	{
		min = glm::min(min, p);
		max = glm::max(max, p);
	}

	void expand(const AABB& other)
	{
		min = glm::min(min, other.min);
		max = glm::max(max, other.max);
	}

	void pad(Type amount)
	{
		vectype v(amount);
		min -= v;
		max += v;
	}

	// -------------------------------------------------
	// Containment
	// -------------------------------------------------

	bool contains(const vectype& p) const
	{
		for (uint8_t i = 0; i < Dim; ++i)
		{
			if (p[i] < min[i] || p[i] > max[i])
				return false;
		}
		return true;
	}

	bool contains(const AABB& other) const { return contains(other.min) && contains(other.max); }

	// -------------------------------------------------
	// Intersection
	// -------------------------------------------------

	bool intersects(const AABB& other) const
	{
		for (uint8_t i = 0; i < Dim; ++i)
		{
			if (other.min[i] > max[i] || other.max[i] < min[i])
				return false;
		}
		return true;
	}

	AABB intersection(const AABB& other) const
	{
		AABB result(glm::max(min, other.min), glm::min(max, other.max));

		if (!result.valid())
			return AABB();

		return result;
	}

	// -------------------------------------------------
	// Ray intersection (slab method, Dim-agnostic)
	// -------------------------------------------------

	bool intersectsRay(const vectype& rayOrigin, const vectype& rayDir, Type& tMin,
					   Type& tMax) const
	{
		tMin = Type(0);
		tMax = std::numeric_limits<Type>::max();

		const Type eps = Type(1e-8);

		for (uint8_t i = 0; i < Dim; ++i)
		{
			if (std::abs(rayDir[i]) < eps)
			{
				// Ray parallel to slab
				if (rayOrigin[i] < min[i] || rayOrigin[i] > max[i])
					return false;
			}
			else
			{
				Type invD = Type(1) / rayDir[i];
				Type t0 = (min[i] - rayOrigin[i]) * invD;
				Type t1 = (max[i] - rayOrigin[i]) * invD;

				if (t0 > t1)
					std::swap(t0, t1);

				tMin = t0 > tMin ? t0 : tMin;
				tMax = t1 < tMax ? t1 : tMax;

				if (tMin > tMax)
					return false;
			}
		}

		return true;
	}
};
using AABB2i = AABB<2, int32_t>;
using AABB3i = AABB<3, int32_t>;
using AABB2f = AABB<2, float>;
using AABB3f = AABB<3, float>;
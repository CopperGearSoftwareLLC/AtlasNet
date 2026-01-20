#pragma once
#include "AtlasObject.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "Transform.hpp"
#include "Types/AABB.hpp"
#include "pch.hpp"

struct Transform : AtlasObject
{
	using WorldIndex = uint16_t;
	WorldIndex world;
	vec3 position;
	AABB3f boundingBox;	 // In Model Space

	void Serialize(ByteWriter& bw) const override
	{
		bw.u16(world);
		bw.vec3(position);
		boundingBox.Serialize(bw);
	}
	void Deserialize(ByteReader& br) override
	{
		world = br.u16();
		position = br.vec3();
		boundingBox.Deserialize(br);
	}
};
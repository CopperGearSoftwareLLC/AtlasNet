#pragma once
#include "Global/AtlasObject.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/Types/AABB.hpp"
#include "Global/pch.hpp"
#include "Transform.hpp"

struct Transform : AtlasObject
{
	using WorldIndex = uint16_t;
	WorldIndex world = 0;
	vec3 position = vec3(0.0f);
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
	std::string ToString() const
	{
		return std::format("Pos: {}, World: {}, AABB: [{}]", glm::to_string(position), world,
						   boundingBox.ToString());
	}
};
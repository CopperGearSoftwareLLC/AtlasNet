#pragma once
#include <cstdint>
#include <span>
#include <vector>

#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
struct IBounds
{
	using BoundsID = uint32_t;
	BoundsID ID;
	virtual void Serialize(ByteWriter& bw) const { bw.write_scalar(ID); };
	virtual void Deserialize(ByteReader& br) { ID = br.read_scalar<BoundsID>(); }

	virtual bool Contains(vec3 p) const = 0;
	auto GetID() const { return ID; }
};
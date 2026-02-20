#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
struct IBounds
{
	virtual ~IBounds() = default;
	using BoundsID = uint32_t;
	BoundsID ID;
	void Serialize(ByteWriter& bw) const
	{
		bw.write_scalar(ID);
		Internal_SerializeData(bw);
	};
	void Deserialize(ByteReader& br)
	{
		ID = br.read_scalar<BoundsID>();
		Internal_DeserializeData(br);
	}

	virtual bool Contains(vec3 p) const = 0;
	auto GetID() const { return ID; }
		protected:
	virtual void Internal_SerializeData(ByteWriter& bw) const = 0;
	virtual void Internal_DeserializeData(ByteReader& br) = 0;
};

struct DummyBounds : IBounds
{
	bool Contains(vec3 p) const override
	{
		throw std::runtime_error("This should never be called");
	};
};
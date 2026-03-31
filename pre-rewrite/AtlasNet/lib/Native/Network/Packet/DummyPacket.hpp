#pragma once

#include "Packet.hpp"

class DummyPacket : public TPacket<DummyPacket, PacketType::eDummy>
{
	int a_number = 69;

	void SerializeData(ByteWriter& bw) const override { bw.i32(a_number); }
	void DeserializeData(ByteReader& br) override { a_number = br.i32(); }
	bool ValidateData() const override { return true; }
};
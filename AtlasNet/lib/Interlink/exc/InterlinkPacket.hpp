#pragma once
#include "InterlinkEnums.hpp"
#include "pch.hpp"
#include "InterlinkIdentifier.hpp"
struct InterlinkPacketHeader
{
	uint32_t packetID;
	InterlinkPacketType packetType;
};

class IInterlinkPacket
{
	InterlinkPacketHeader header;
  public:
	virtual void Serialize(std::vector<std::byte> &data) const = 0;
	virtual bool Deserialize(const std::vector<std::byte> &data) = 0;
};

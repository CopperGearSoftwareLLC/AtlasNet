#pragma once
#include "pch.hpp"


struct InterlinkPacket
{
	uint32_t packetID;
	size_t size;
	std::byte data[];
};

template <typename SubPacket>
std::pair<std::unique_ptr<InterlinkPacket>, SubPacket &> MakeInterlinkPacket()
{
	char *data = new char[sizeof(InterlinkPacket) + sizeof(SubPacket)];
	new (data) InterlinkPacket();
	std::unique_ptr<InterlinkPacket> Packet =
		std::unique_ptr<InterlinkPacket>(reinterpret_cast<InterlinkPacket *>(data));
	return {Packet, Packet->data};
}
#pragma once
#include "InterlinkEnums.hpp"
#include "pch.hpp"
struct InterlinkPacketHeader
{
	uint32_t packetID;
	InterlinkPacketType packetType;
};
class IInterlinkPacket
{
  public:
	virtual void Serialize(std::vector<std::byte> &data) const = 0;
	virtual void Deserialize(const std::vector<std::byte> &data) = 0;
};

class InterlinkPacketWrap : public IInterlinkPacket
{
public:
	std::shared_ptr<IInterlinkPacket> SubPacket;
	template <typename T>
	std::weak_ptr<T> CreateSubPacket()
	{
		std::shared_ptr<T> ptr = std::make_shared<T>();
		SubPacket = ptr;
		return ptr;
	}
};

class InterlinkRelayPacket : public InterlinkPacketWrap
{

};
class InterlinkDataPacket : public IInterlinkPacket
{
	size_t DataSize;
	std::unique_ptr<std::byte[]> RawData;

  public:
	void SetData(void *data, size_t size)
	{
		DataSize = size;
		RawData = std::make_unique<std::byte[]>(size);
		std::memcpy(RawData.get(), data, size);
	}

};



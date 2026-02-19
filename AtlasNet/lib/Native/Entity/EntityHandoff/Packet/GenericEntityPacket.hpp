#pragma once

// Generic network packet carrying sender identity and full AtlasEntity payload.

#include <cstdint>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"

class GenericEntityPacket
	: public TPacket<GenericEntityPacket, "GenericEntityPacket">
{
  public:
	static constexpr uint8_t kProtocolVersion = 2;

	NetworkIdentity sender;
	AtlasEntity entity;
	uint8_t protocolVersion = kProtocolVersion;
	uint64_t transferTick = 0;
	uint64_t sentAtMs = 0;

  private:
	void SerializeData(ByteWriter& writer) const override
	{
		sender.Serialize(writer);
		entity.Serialize(writer);
		writer.write_scalar(protocolVersion);
		writer.write_scalar(transferTick);
		writer.write_scalar(sentAtMs);
	}

	void DeserializeData(ByteReader& reader) override
	{
		sender.Deserialize(reader);
		entity.Deserialize(reader);
		protocolVersion = reader.read_scalar<uint8_t>();
		transferTick = reader.read_scalar<uint64_t>();
		sentAtMs = reader.read_scalar<uint64_t>();
	}

	[[nodiscard]] bool ValidateData() const override
	{
		return sender.Type != NetworkIdentityType::eInvalid;
	}
};

ATLASNET_REGISTER_PACKET(GenericEntityPacket, "GenericEntityPacket");

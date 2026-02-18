#pragma once

#include <cstdint>

#include "Entity.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"

class HandoffEntityPacket
	: public TPacket<HandoffEntityPacket, "HandoffEntityPacket">
{
  public:
	NetworkIdentity sender;
	AtlasEntityMinimal entity;
	uint64_t sentAtMs = 0;

  private:
	void SerializeData(ByteWriter& writer) const override
	{
		sender.Serialize(writer);
		entity.Serialize(writer);
		writer.write_scalar(sentAtMs);
	}

	void DeserializeData(ByteReader& reader) override
	{
		sender.Deserialize(reader);
		entity.Deserialize(reader);
		sentAtMs = reader.read_scalar<uint64_t>();
	}

	[[nodiscard]] bool ValidateData() const override
	{
		return sender.Type != NetworkIdentityType::eInvalid;
	}
};

ATLASNET_REGISTER_PACKET(HandoffEntityPacket, "HandoffEntityPacket");

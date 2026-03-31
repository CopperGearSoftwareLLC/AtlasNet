#pragma once

#include "Global/Misc/UUID.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
class ClientIDAssignPacket
	: public TPacket<ClientIDAssignPacket, "ClientIDAssignPacket">
{
	void SerializeData(ByteWriter& bw) const override
	{
		AssignedClientID.Serialize(bw);
	}
	void DeserializeData(ByteReader& br) override
	{
		AssignedClientID.Deserialize(br);
	}
	[[nodiscard]] bool ValidateData() const override
	{
		return !AssignedClientID.ID.is_nil();
	}

   public:
	NetworkIdentity AssignedClientID;
};
ATLASNET_REGISTER_PACKET(ClientIDAssignPacket);
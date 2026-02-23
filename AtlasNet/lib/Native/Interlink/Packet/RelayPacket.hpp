#pragma once
#include <memory>

#include "InterlinkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
#include "Packet/CommandPacket.hpp"
#include "Serialize/ByteWriter.hpp"
class RelayPacket : public TPacket<RelayPacket, "RelayPacket">
{
	NetworkIdentity FinalTarget;
	std::shared_ptr<IPacket> sub_Packet;

   public:
	RelayPacket() : TPacket() {}
	RelayPacket& SetFinalTarget(const NetworkIdentity& id)
	{
		FinalTarget = id;
		return *this;
	}
	template <typename T>
	RelayPacket& SetSubPacket(const T& packet)
	{
		sub_Packet = std::make_shared<T>(packet);
		return *this;
	}

   private:
	void SerializeData(ByteWriter& bw) const override
	{
		FinalTarget.Serialize(bw);
		sub_Packet->Serialize(bw);
	}
	void DeserializeData(ByteReader& br) override
	{
		FinalTarget.Deserialize(br);
		sub_Packet->Deserialize(br);
	}
	bool ValidateData() const override
	{
		return (FinalTarget.Type != NetworkIdentityType::eInvalid) && sub_Packet &&
			   sub_Packet->Validate();
	}
};

void Test()
{
	uint32 BoundId = 52;
	CommandPacket cmdPacket;
	cmdPacket
    .SetCommandType(CommandPacket::Type::eFetchNewBounds)
		.WriteArgs(
            [BoundId = BoundId](ByteWriter& bw)
             { bw.write_scalar(BoundId); });

	RelayPacket relayPacket;
	relayPacket
    .SetFinalTarget(NetworkIdentity::MakeIDWatchDog())
    .SetSubPacket(cmdPacket);
}
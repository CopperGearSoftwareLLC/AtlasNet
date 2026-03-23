#pragma once

#include "Heuristic/IBounds.hpp"
#include "Network/Packet/Packet.hpp"

class TemporaryMigrationTriggerPacket
	: public TPacket<TemporaryMigrationTriggerPacket, "TemporaryMigrationTriggerPacket">
{
   public:
	BoundsID releasedBoundID = 0;

   protected:
	void SerializeData(ByteWriter& bw) const override
	{
		bw.write_scalar(releasedBoundID);
	}

	void DeserializeData(ByteReader& br) override
	{
		releasedBoundID = br.read_scalar<BoundsID>();
	}

	[[nodiscard]] bool ValidateData() const override { return true; }
};

ATLASNET_REGISTER_PACKET(TemporaryMigrationTriggerPacket);

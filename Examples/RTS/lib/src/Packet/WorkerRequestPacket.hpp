#pragma once

#include "GameData.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/Packet/Packet.hpp"
#include "Network/Packet/PacketManager.hpp"
class WorkerRequestPacket : public TPacket<WorkerRequestPacket, "WorkerRequestPacket">
{
    public:
	enum class State
	{
		eRequest,
		eResponse
	};
	boost::container::small_vector<WorkerData, 5> Workers;
	State state;

	void SerializeData(ByteWriter& serializer) const override
	{
		serializer.write_scalar(state);
		serializer.u64(Workers.size());
		for (int i = 0; i < Workers.size(); i++)
		{
			Workers[i].Serialize(serializer);
		}
	}

	void DeserializeData(ByteReader& deserializer) override
	{
		state = deserializer.read_scalar<State>();
		uint64_t workerCount = deserializer.u64();
		Workers.resize(workerCount);
		for (int i = 0; i < workerCount; i++)
		{
			Workers[i].Deserialize(deserializer);
		}
	}
	bool ValidateData() const override
	{
		return true;
	}
};
ATLASNET_REGISTER_PACKET(WorkerRequestPacket);
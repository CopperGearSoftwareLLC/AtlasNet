#pragma once


#include <boost/container/flat_map.hpp>
#include "EntityID.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/Packet/Packet.hpp"
#include "Network/Packet/PacketManager.hpp"
class WorkerMoveNotify : public TPacket<WorkerMoveNotify, "WorkerMoveNotify">
{
public:
    boost::container::small_flat_map<EntityID, vec3, 15> WorkerPositions;

    void SerializeData(ByteWriter& serializer) const override
    {
        serializer.u64(WorkerPositions.size());
        for (const auto& [id, pos] : WorkerPositions)
        {
            serializer.uuid(id);
            serializer.vec3(pos);
        }
    }
    void DeserializeData(ByteReader& deserializer) override
    {
        uint64_t count = deserializer.u64();
        for (int i = 0; i < count; i++)
        {
            EntityID id = deserializer.uuid();
            vec3 pos = deserializer.vec3();
            WorkerPositions[id] = pos;
        }
    }
    bool ValidateData() const override
    {
        return true;
    }
};
ATLASNET_REGISTER_PACKET(WorkerMoveNotify);
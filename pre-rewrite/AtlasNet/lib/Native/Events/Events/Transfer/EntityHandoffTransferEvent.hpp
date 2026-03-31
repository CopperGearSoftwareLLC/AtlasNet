#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
#include "Transfer/TransferData.hpp"

struct EntityHandoffTransferEvent : public IEvent
{
	std::string transferId;
	std::string fromId;
	std::string toId;
	EntityTransferStage stage = EntityTransferStage::eNone;
	std::string state;
	std::vector<std::string> entityIds;
	uint64_t timestampMs = 0;

	void Serialize(ByteWriter& bw) const override
	{
		bw.str(transferId);
		bw.str(fromId);
		bw.str(toId);
		bw.write_scalar<EntityTransferStage>(stage);
		bw.str(state);
		bw.var_u32(static_cast<uint32_t>(entityIds.size()));
		for (const auto& entityId : entityIds)
		{
			bw.str(entityId);
		}
		bw.u64(timestampMs);
	}

	void Deserialize(ByteReader& br) override
	{
		transferId = br.str();
		fromId = br.str();
		toId = br.str();
		stage = br.read_scalar<EntityTransferStage>();
		state = br.str();

		const uint32_t entityCount = br.var_u32();
		entityIds.clear();
		entityIds.reserve(entityCount);
		for (uint32_t i = 0; i < entityCount; i++)
		{
			entityIds.push_back(br.str());
		}
		timestampMs = br.u64();
	}
};

ATLASNET_REGISTER_EVENT(EntityHandoffTransferEvent);

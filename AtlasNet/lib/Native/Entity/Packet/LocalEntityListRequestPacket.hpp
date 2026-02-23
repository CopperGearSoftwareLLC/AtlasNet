#pragma once

#include <variant>
#include <vector>

#include "Entity/Entity.hpp"
#include "Network/Packet/Packet.hpp"
class LocalEntityListRequestPacket
	: public TPacket<LocalEntityListRequestPacket, "LocalEntityListRequestPacket">
{
    public:
	enum MsgStatus
	{
		eQuery,
		eResponse
	} status;
	bool Request_IncludeMetadata;

	std::variant<std::vector<AtlasEntity>, std::vector<AtlasEntityMinimal>> Response_Entities;

	void SerializeData(ByteWriter& bw) const override
	{
		bw.write_scalar(status);
		bw.i8(Request_IncludeMetadata);
		std::visit(
			[&](const auto& vec)  // capture bw by reference
			{
				bw.u64(vec.size());
				using T = typename std::decay_t<decltype(vec)>::value_type;
				for (const T& e : vec)	// const because vec is const&
				{
					e.Serialize(bw);
				}
			},
			Response_Entities);
	}
	void DeserializeData(ByteReader& br) override
	{
		status = br.read_scalar<MsgStatus>();
		Request_IncludeMetadata = br.i8();
		size_t entityCount = br.u64();
		if (Request_IncludeMetadata)
		{
			auto& vecA = Response_Entities.emplace<std::vector<AtlasEntity>>();
			vecA.reserve(entityCount);
			// Fill vecA
			for (int i = 0; i < entityCount; i++)
			{
				AtlasEntity e;
				e.Deserialize(br);
				vecA.push_back(e);
			}
		}
		else
		{
			auto& vecB = Response_Entities.emplace<std::vector<AtlasEntityMinimal>>();
			// Fill vecB
			vecB.reserve(entityCount);

			for (int i = 0; i < entityCount; i++)
			{
				AtlasEntityMinimal e;
				e.Deserialize(br);
				vecB.push_back(e);
			}
		}
	}
	[[nodiscard]] bool ValidateData() const override
	{
		if (status == MsgStatus::eQuery)
			return true;

		if (status == MsgStatus::eResponse)
		{
			if (Request_IncludeMetadata)
			{
				// Must hold full AtlasEntity
				return std::holds_alternative<std::vector<AtlasEntity>>(Response_Entities);
			}
			else
			{
				return std::holds_alternative<std::vector<AtlasEntityMinimal>>(Response_Entities);
			}
		}

		return false;  // any other status
	}
};
ATLASNET_REGISTER_PACKET(LocalEntityListRequestPacket, "LocalEntityListRequestPacket");
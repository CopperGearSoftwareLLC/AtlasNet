#pragma once

#include <boost/describe/enum.hpp>
#include <variant>

#include "Entity/Entity.hpp"
#include "Network/Packet/Packet.hpp"
class EntityHandleFetchRequestPacket
	: public TPacket<EntityHandleFetchRequestPacket, "EntityHandleFetchRequestPacket">
{
   public:
	enum class State
	{
		eRequest = 0,
		eResponse = 1
	};
	BOOST_DESCRIBE_NESTED_ENUM(State,eRequest,eResponse)

	struct PacketData
	{
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	struct RequestData : public PacketData
	{
		AtlasEntityID entityID;
		void Serialize(ByteWriter& bw) const override { bw.uuid(entityID); }
		void Deserialize(ByteReader& br) override { entityID = br.uuid(); }
	};
	struct ResponseData : public PacketData
	{
		AtlasEntity entityData;
		void Serialize(ByteWriter& bw) const override { entityData.Serialize(bw); }
		void Deserialize(ByteReader& br) override { entityData.Deserialize(br); }
	};

	std::variant<RequestData, ResponseData> data;
	State currentState;
	void SerializeData(ByteWriter& bw) const override
	{
		bw.u8(static_cast<uint8_t>(currentState));
		std::visit([&bw](const auto& d) { d.Serialize(bw); }, data);
	}
	void DeserializeData(ByteReader& br) override
	{
		currentState = static_cast<State>(br.u8());
		if (currentState == State::eRequest)
		{
			RequestData requestData;
			requestData.Deserialize(br);
			data = requestData;
		}
		else if (currentState == State::eResponse)
		{
			ResponseData responseData;
			responseData.Deserialize(br);
			data = responseData;
		}
	}
    bool ValidateData() const override
    {
        if (currentState == State::eRequest)
        {
            return std::holds_alternative<RequestData>(data);
        }
        else if (currentState == State::eResponse)
        {
            return std::holds_alternative<ResponseData>(data);
        }
        return false;
    }
	const RequestData& GetRequestData() const
	{
		if (currentState != State::eRequest)
			throw std::runtime_error("Packet is not in Request state");
		return std::get<RequestData>(data);
	}
	const ResponseData& GetResponseData() const
	{
		if (currentState != State::eResponse)
			throw std::runtime_error("Packet is not in Response state");
		return std::get<ResponseData>(data);
	}

};

ATLASNET_REGISTER_PACKET(EntityHandleFetchRequestPacket);
#pragma once


#pragma once
#include <boost/container/small_vector.hpp>
#include <cstddef>
#include <cstdint>
#include <variant>

#include "Client/Client.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
#include "Transfer/TransferData.hpp"

class ClientSwitchPacket : public TPacket<ClientSwitchPacket, "ClientSwitchPacket">
{
   public:
	ClientSwitchPacket() = default;
	struct StageData
	{
		virtual ~StageData() = default;
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	
	struct RequestSwitchStageData : StageData
	{
		boost::container::small_vector<ClientID, 5> clientIDs;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(clientIDs.size());
			for (const auto& clientID : clientIDs)
			{
				bw.uuid(clientID);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			clientIDs.resize(br.u64());
			for (uint64_t i = 0; i < clientIDs.size(); i++)
			{
				clientIDs[i] = br.uuid();
			}
		}
	};
	const RequestSwitchStageData& GetAsRequestSwitchStage() const
	{
		return std::get<RequestSwitchStageData>(Data);
	}
	RequestSwitchStageData& SetAsRequestSwitchStage()
	{
		return Data.emplace<RequestSwitchStageData>();
	}

	struct FreezeStageData : StageData
	{
		void Serialize(ByteWriter& bw) const override {}
		void Deserialize(ByteReader& br) override {}
	};
	const FreezeStageData& GetAsFreezeStage() const { return std::get<FreezeStageData>(Data); }
	FreezeStageData& SetAsFreezeStage() { return Data.emplace<FreezeStageData>(); }

	

	struct ActivateStageData : StageData
	{
		void Serialize(ByteWriter& bw) const override {}
		void Deserialize(ByteReader& br) override {}
	};
	const ActivateStageData& GetAsActivateStage() const
	{
		return std::get<ActivateStageData>(Data);
	}
	ActivateStageData& SetAsActivateStage() { return Data.emplace<ActivateStageData>(); }

	UUID TransferID;
	ClientSwitchStage stage;
	std::variant<RequestSwitchStageData, FreezeStageData, ActivateStageData>
		Data;
	void SerializeData(ByteWriter& bw) const override
	{
		bw.uuid(TransferID);
		bw.write_scalar<ClientSwitchStage>(stage);
		std::visit([&bw](auto const& stageData) { stageData.Serialize(bw); }, Data);
	};
	void DeserializeData(ByteReader& br) override
	{
		TransferID = br.uuid();
		stage = br.read_scalar<ClientSwitchStage>();
		switch (stage)
		{

			case ClientSwitchStage::eRequestSwitch:
				Data.emplace<RequestSwitchStageData>();
				break;
			case ClientSwitchStage::eFreeze:
				Data.emplace<FreezeStageData>();
				break;
			case ClientSwitchStage::eActivate:
				Data.emplace<ActivateStageData>();
				break;
		}
		// 4️⃣ Deserialize into the constructed variant
		std::visit([&br](auto& stageData) { stageData.Deserialize(br); }, Data);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
};
ATLASNET_REGISTER_PACKET(ClientSwitchPacket);
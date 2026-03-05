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

class ClientTransferPacket : public TPacket<ClientTransferPacket, "ClientTransferPacket">
{
   public:
	ClientTransferPacket() = default;
	struct StageData
	{
		virtual ~StageData() = default;
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	struct PrepareStageData : StageData
	{
		boost::container::small_vector<std::pair<ClientID, AtlasEntityID>, 5> clients;

		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(clients.size());
			for (const auto& [clientID, entityID] : clients)
			{
				bw.uuid(clientID);
				bw.uuid(entityID);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			clients.resize(br.u64());
			for (uint64_t i = 0; i < clients.size(); i++)
			{
				clients[i].first = br.uuid();
				clients[i].second = br.uuid();
			}
		}
	};
	const PrepareStageData& GetAsPrepareStage() const { return std::get<PrepareStageData>(Data); }
	PrepareStageData& SetAsPrepareStage() { return Data.emplace<PrepareStageData>(); }

	struct ReadyStageData : StageData
	{
		void Serialize(ByteWriter& bw) const override {}
		void Deserialize(ByteReader& br) override {}
	};
	const ReadyStageData& GetAsReadyStage() const { return std::get<ReadyStageData>(Data); }
	ReadyStageData& SetAsReadyStage() { return Data.emplace<ReadyStageData>(); }

	struct DrainedStageData : StageData
	{
		boost::container::small_vector<AtlasEntity, 5> clientPayloads;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(clientPayloads.size());
			for (const auto& clientEntity : clientPayloads)
			{
				clientEntity.Serialize(bw);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			clientPayloads.resize(br.u64());
			for (uint64_t i = 0; i < clientPayloads.size(); i++)
			{
				clientPayloads[i].Deserialize(br);
			}
		}
	};
	const DrainedStageData& GetAsDrainedStage() const { return std::get<DrainedStageData>(Data); }
	DrainedStageData& SetAsDrainedStage() { return Data.emplace<DrainedStageData>(); }

	UUID TransferID;
	ClientTransferStage stage;
	std::variant<PrepareStageData, ReadyStageData, DrainedStageData> Data;
	void SerializeData(ByteWriter& bw) const override
	{
		bw.uuid(TransferID);
		bw.write_scalar<ClientTransferStage>(stage);
		std::visit([&bw](auto const& stageData) { stageData.Serialize(bw); }, Data);
	};
	void DeserializeData(ByteReader& br) override
	{
		TransferID = br.uuid();
		stage = br.read_scalar<ClientTransferStage>();
		switch (stage)
		{
			case ClientTransferStage::ePrepare:
				Data.emplace<PrepareStageData>();
				break;
			case ClientTransferStage::eReady:
				Data.emplace<ReadyStageData>();
				break;
			case ClientTransferStage::eDrained:
				Data.emplace<DrainedStageData>();
				break;
		}
		// 4️⃣ Deserialize into the constructed variant
		std::visit([&br](auto& stageData) { stageData.Deserialize(br); }, Data);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
};
ATLASNET_REGISTER_PACKET(ClientTransferPacket);
#pragma once
#include <cstddef>
#include <cstdint>
#include <variant>

#include "Entity/Entity.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"

class ClientTransferPacket : public TPacket<ClientTransferPacket, "ClientTransferPacket">
{
	public:
	enum class MsgStage	 // A = From Shard, B = To Shard
	{
		eShardPrepare,		  // A -> B notify of intent to transfer
		eShardReady,		  // B -> A notifying readiness to receive transfer,
		eProxyRequestSwitch,  // A -> Proxy request
		eProxyFreeze,		  // Proxy -> A  Notification of request approval and stream freeze
		eShardDrained,	// A -> Proxy notification of completion on processing lingering packets
		eProxyTransferActivate,	 // Proxy -> B, Transfer complete, ownership transferred
	};

	struct StageData
	{
		virtual ~StageData() = default;
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	struct PrepareStageData : StageData
	{
		struct EntityData
		{
			AtlasEntity LastEntitySnapshot;
			uint64_t LastPacketSequence;
		};
		boost::container::small_vector<EntityData,10> entitiesToTransfer;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				ed.LastEntitySnapshot.Serialize(bw);
				bw.u64(ed.LastPacketSequence);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				EntityData ed;
				ed.LastEntitySnapshot.Deserialize(br);
				ed.LastPacketSequence = br.u64();
				entitiesToTransfer[i] = ed;
			}
		}
	};
	struct ReadyStageData : StageData
	{
		struct EntityData
		{
			AtlasEntityID EntityID;
			uint64_t LastPacketSequence;
		};
		boost::container::small_vector<EntityData,10> entitiesToTransfer;

		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				bw.uuid(ed.EntityID);
				bw.u64(ed.LastPacketSequence);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				EntityData ed;
				ed.EntityID = br.uuid();
				ed.LastPacketSequence = br.u64();
				entitiesToTransfer[i] = ed;
			}
		}
	};
	struct RequestSwitchStageData : StageData
	{
		boost::container::small_vector<AtlasEntityID,10> entitiesToTransfer;
		NetworkIdentity newOwner;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				bw.uuid(ed);
			}
			newOwner.Serialize(bw);
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				entitiesToTransfer[i] = br.uuid();
			}
			newOwner.Deserialize(br);
		}
	};
	struct FreezeStageData : StageData
	{
		boost::container::small_vector<AtlasEntityID,10> entitiesToTransfer;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				bw.uuid(ed);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				entitiesToTransfer[i] = br.uuid();
			}
		}
	};
	struct DrainedStageData : StageData
	{
		struct EntityData
		{
			AtlasEntityID EntityID;
			uint64_t LastPacketSequence;
		};
		boost::container::small_vector<EntityData,10> entitiesToTransfer;

		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				bw.uuid(ed.EntityID);
				bw.u64(ed.LastPacketSequence);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				EntityData ed;
				ed.EntityID = br.uuid();
				ed.LastPacketSequence = br.u64();
				entitiesToTransfer[i] = ed;
			}
		}
	};
	struct TransferActivateStageData : StageData
	{
		struct EntityData
		{
			AtlasEntityID EntityID;
			uint64_t LastPacketSequence;
			uint64_t EntityGeneration;
		};
		boost::container::small_vector<EntityData,10> entitiesToTransfer;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(entitiesToTransfer.size());
			for (const auto& ed : entitiesToTransfer)
			{
				bw.uuid(ed.EntityID);
				bw.u64(ed.LastPacketSequence);
				bw.u64(ed.EntityGeneration);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			entitiesToTransfer.resize(br.u64());
			for (uint64_t i = 0; i < entitiesToTransfer.size(); i++)
			{
				EntityData ed;
				ed.EntityID = br.uuid();
				ed.LastPacketSequence = br.u64();
				ed.EntityGeneration = br.u64();
				entitiesToTransfer[i] = ed;
			}
		}
	};

   
	UUID TransferID;
	MsgStage stage;
	std::variant<PrepareStageData, ReadyStageData, RequestSwitchStageData, FreezeStageData,
				 DrainedStageData, TransferActivateStageData>
		Data;

   private:
	void SerializeData(ByteWriter& bw) const override
	{
		bw.uuid(TransferID);
		bw.write_scalar<MsgStage>(stage);

		// 3️⃣ Serialize correct variant
		std::visit([&bw](auto const& stageData) { stageData.Serialize(bw); }, Data);
	};
	void DeserializeData(ByteReader& br) override
	{
		TransferID = br.uuid();
		stage = br.read_scalar<MsgStage>();
		// 3️⃣ Construct correct variant type
		switch (stage)
		{
			case MsgStage::eShardPrepare:
				Data.emplace<PrepareStageData>();
				break;

			case MsgStage::eShardReady:
				Data.emplace<ReadyStageData>();
				break;

			case MsgStage::eProxyRequestSwitch:
				Data.emplace<RequestSwitchStageData>();
				break;

			case MsgStage::eProxyFreeze:
				Data.emplace<FreezeStageData>();
				break;

			case MsgStage::eShardDrained:
				Data.emplace<DrainedStageData>();
				break;

			case MsgStage::eProxyTransferActivate:
				Data.emplace<TransferActivateStageData>();
				break;

			default:
				throw std::runtime_error("Invalid ClientTransferPacket stage");
		}
		// 4️⃣ Deserialize into the constructed variant
		std::visit([&br](auto& stageData) { stageData.Deserialize(br); }, Data);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
};
ATLASNET_REGISTER_PACKET(ClientTransferPacket, "ClientTransferPacket");
#pragma once

#include <steam/steamtypes.h>

#include <boost/container/small_vector.hpp>
#include <variant>

#include "Client/Client.hpp"
#include "Entity/Transform.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
struct ClientSpawnPacket : public TPacket<ClientSpawnPacket, "ClientSpawnPacket">
{
	enum Stage
	{
		eNotification,
		eAcknowledge
	};

	struct StageData
	{
		virtual ~StageData() = default;
		virtual void Serialize(ByteWriter& bw) const = 0;
		virtual void Deserialize(ByteReader& br) = 0;
	};
	struct NotificationData : StageData
	{
		struct NewClientData
		{
			Client client;
			Transform spawn_Location;
		};
		boost::container::small_vector<NewClientData, 10> incomingClients;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(incomingClients.size());
			for (const auto& cd : incomingClients)
			{
				cd.client.Serialize(bw);
				cd.spawn_Location.Serialize(bw);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			incomingClients.resize(br.u64());
			for (uint64 i = 0; i < incomingClients.size(); i++)
			{
				NewClientData& d = incomingClients[i];
				d.client.Deserialize(br);
				d.spawn_Location.Deserialize(br);
			}
		}
	};
	const NotificationData& GetAsNotification() const { return std::get<NotificationData>(Data); }
	NotificationData& SetAsNotification() { return Data.emplace<NotificationData>(); }

	struct AcknowledgeData : StageData
	{
		boost::container::small_vector<ClientID, 10> AcceptedClients;
		void Serialize(ByteWriter& bw) const override
		{
			bw.u64(AcceptedClients.size());
			for (const auto& cd : AcceptedClients)
			{
				bw.uuid(cd);
			}
		}
		void Deserialize(ByteReader& br) override
		{
			AcceptedClients.resize(br.u64());
			for (uint64 i = 0; i < AcceptedClients.size(); i++)
			{
				AcceptedClients[i] = br.uuid();
			}
		}
	};
	const AcknowledgeData& GetAsAcknowledge() const { return std::get<AcknowledgeData>(Data); }
	AcknowledgeData& SetAsAcknowledge() { return Data.emplace<AcknowledgeData>(); }

	Stage stage;
	std::variant<NotificationData, AcknowledgeData> Data;

	void SerializeData(ByteWriter& bw) const override
	{
		bw.write_scalar<Stage>(stage);
		std::visit([&bw](auto const& stageData) { stageData.Serialize(bw); }, Data);
	};
	void DeserializeData(ByteReader& br) override
	{
		stage = br.read_scalar<Stage>();
		switch (stage)
		{
			case Stage::eNotification:
				Data.emplace<NotificationData>();
				break;

			case Stage::eAcknowledge:
				Data.emplace<AcknowledgeData>();
				break;

			default:
				throw std::runtime_error("Invalid ClientTransferPacket stage");
		}
		// 4️⃣ Deserialize into the constructed variant
		std::visit([&br](auto& stageData) { stageData.Deserialize(br); }, Data);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
};
ATLASNET_REGISTER_PACKET(ClientSpawnPacket);
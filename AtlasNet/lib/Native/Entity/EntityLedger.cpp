#include "EntityLedger.hpp"

#include <algorithm>
#include <chrono>
#include <stop_token>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Entity/Transfer/TransferCoordinator.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/Packet/PacketManager.hpp"
void EntityLedger::Init()
{
	sub_EntityListRequestPacket =
		Interlink::Get().GetPacketManager().Subscribe<LocalEntityListRequestPacket>(
			[this](const LocalEntityListRequestPacket& p, const PacketManager::PacketInfo& info)
			{
				if (p.status == LocalEntityListRequestPacket::MsgStatus::eQuery)
				{
					OnLocalEntityListRequest(p, info);
				}
				else
				{
				}
			});
	TransferCoordinator::Get();
	LoopThread = std::jthread([this](std::stop_token st) { LoopThreadEntry(st); });
};
void EntityLedger::OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
											const PacketManager::PacketInfo& info)
{
	// logger.DebugFormatted("received a EntityList request from {}", info.sender.ToString());
	LocalEntityListRequestPacket response;
	response.status = LocalEntityListRequestPacket::MsgStatus::eResponse;

	if (p.Request_IncludeMetadata)
	{
		auto& vec = response.Response_Entities.emplace<std::vector<AtlasEntity>>();
		_ReadLock(
			[&]()
			{
				for (const auto& [ID, entity] : entities)
				{
					vec.emplace_back(entity);
				}
			});
	}
	else
	{
		auto& vec = response.Response_Entities.emplace<std::vector<AtlasEntityMinimal>>();
		_ReadLock(
			[&]()
			{
				for (const auto& [ID, entity] : entities)
				{
					vec.emplace_back(static_cast<AtlasEntityMinimal>(entity));
				}
			});
	}
	response.Request_IncludeMetadata = p.Request_IncludeMetadata;
	// logger.DebugFormatted("responding:", info.sender.ToString());
	Interlink::Get().SendMessage(info.sender, response, NetworkMessageSendFlag::eUnreliableNow);
}
void EntityLedger::LoopThreadEntry(std::stop_token st)
{
	while (!st.stop_requested())
	{
		boost::container::small_vector<AtlasEntityID, 32> EntitiesNewlyOutOfBounds;
		// copy entity IDs + positions under lock, then release
		std::vector<std::pair<AtlasEntityID, glm::vec3>> snapshot;
		_ReadLock(
			[&]()
			{
				snapshot.reserve(entities.size());
				for (const auto& [ID, entity] : entities)
					snapshot.emplace_back(ID, entity.transform.position);
			});

		const NetworkIdentity selfId = NetworkCredentials::Get().GetID();
		for (const auto& [ID, pos] : snapshot)
		{
			// Skip entities already in an active transfer.
			if (TransferCoordinator::Get().IsEntityInTransfer(ID))
			{
				continue;
			}

			Transform t;
			t.position = pos;
			const auto targetShard = HeuristicManifest::Get().ShardFromPosition(t);
			if (!targetShard.has_value() || *targetShard == selfId)
			{
				continue;
			}

			EntitiesNewlyOutOfBounds.push_back(ID);
			logger.DebugFormatted(
				"Entity scheduled for transfer:\n - ID {}\n - EntityPos: {}\n - targetShard {}",
				UUIDGen::ToString(ID), glm::to_string(pos), targetShard->ToString());
		}

		if (!EntitiesNewlyOutOfBounds.empty())
		{
			TransferCoordinator::Get().MarkEntitiesForTransfer(std::span(EntitiesNewlyOutOfBounds));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

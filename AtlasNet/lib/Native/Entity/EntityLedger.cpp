#include "EntityLedger.hpp"

#include <algorithm>
#include <chrono>
#include <stop_token>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Entity/TransferCoordinator.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
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
	sub_ClientTransferPacket = Interlink::Get().GetPacketManager().Subscribe<ClientTransferPacket>(
		[this](const ClientTransferPacket& p, const PacketManager::PacketInfo& info)
		{ onClientTransferPacket(p, info); });

	sub_EntityTransferPacket = Interlink::Get().GetPacketManager().Subscribe<EntityTransferPacket>(
		[this](const EntityTransferPacket& p, const PacketManager::PacketInfo& info)
		{ onEntityTransferPacket(p, info); });
	LoopThread = std::jthread([this](std::stop_token st) { LoopThreadEntry(st); });
};
void EntityLedger::OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
											const PacketManager::PacketInfo& info)
{
	logger.DebugFormatted("received a EntityList request from {}", info.sender.ToString());
	LocalEntityListRequestPacket response;
	response.status = LocalEntityListRequestPacket::MsgStatus::eResponse;

	if (p.Request_IncludeMetadata)
	{
		auto& vec = response.Response_Entities.emplace<std::vector<AtlasEntity>>();
		for (const auto& [ID, entity] : entities)
		{
			vec.emplace_back(entity);
		}
	}
	else
	{
		auto& vec = response.Response_Entities.emplace<std::vector<AtlasEntityMinimal>>();
		for (const auto& [ID, entity] : entities)
		{
			vec.emplace_back((AtlasEntityMinimal)entity);
		}
	}
	response.Request_IncludeMetadata = p.Request_IncludeMetadata;
	logger.DebugFormatted("responding:", info.sender.ToString());
	Interlink::Get().SendMessage(info.sender, response, NetworkMessageSendFlag::eReliableNow);
}
void EntityLedger::LoopThreadEntry(std::stop_token st)
{
	while (!st.stop_requested())
	{
		boost::container::small_vector<AtlasEntityID, 32> EntitiesNewlyOutOfBounds;
		if (!BoundLeaser::Get().HasBound())
			continue;
		for (const auto& [ID, entity] : entities)
		{
			if (entitiesMarkedForTransfer.contains(ID))
				continue;
			if (!BoundLeaser::Get().GetBound().Contains(entity.data.transform.position))
			{
				EntitiesNewlyOutOfBounds.push_back(ID);
				entitiesMarkedForTransfer.insert(ID);
			}
		}
		if (!EntitiesNewlyOutOfBounds.empty())
		{
			TransferCoordinator::Get().MarkEntitiesForTransfer(std::span(EntitiesNewlyOutOfBounds));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void EntityLedger::onClientTransferPacket(const ClientTransferPacket& packet,
										  const PacketManager::PacketInfo& info)
{
}
void EntityLedger::onEntityTransferPacket(const EntityTransferPacket& packet,
										  const PacketManager::PacketInfo& info)
{
}

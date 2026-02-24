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
			std::lock_guard<std::mutex> lock(EntityListMutex);
	// logger.DebugFormatted("received a EntityList request from {}", info.sender.ToString());
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
	// logger.DebugFormatted("responding:", info.sender.ToString());
	Interlink::Get().SendMessage(info.sender, response, NetworkMessageSendFlag::eUnreliableNow);
}
void EntityLedger::LoopThreadEntry(std::stop_token st)
{
    while (!st.stop_requested())
    {
        boost::container::small_vector<AtlasEntityID, 32> EntitiesNewlyOutOfBounds;
        if (!BoundLeaser::Get().HasBound())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // copy entity IDs + positions under lock, then release
        std::vector<std::pair<AtlasEntityID, glm::vec3>> snapshot;
        {
            std::lock_guard<std::mutex> lock(EntityListMutex);
            snapshot.reserve(entities.size());
            for (const auto& [ID, entity] : entities)
                snapshot.emplace_back(ID, entity.data.transform.position);
        }

        const auto& Bound = BoundLeaser::Get().GetBound();
        for (const auto& [ID, pos] : snapshot)
        {
            if (!Bound.Contains(pos))
            {
                // we call IsEntityInTransfer *without* holding EntityListMutex
                if (TransferCoordinator::Get().IsEntityInTransfer(ID))
                    continue;

                EntitiesNewlyOutOfBounds.push_back(ID);
                logger.DebugFormatted(
                    "Entity out of bounds:\n - ID {}\n - EntityPos: {}\n - bounds {}",
                    UUIDGen::ToString(ID), glm::to_string(pos), Bound.ToDebugString());
            }
        }

        if (!EntitiesNewlyOutOfBounds.empty())
        {
            TransferCoordinator::Get().MarkEntitiesForTransfer(std::span(EntitiesNewlyOutOfBounds));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

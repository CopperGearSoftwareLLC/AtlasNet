#include "EntityLedger.hpp"

#include <algorithm>
#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <stop_token>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "Transfer/TransferCoordinator.hpp"
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
	sub_EntityHandleFetchRequestPacket =
		Interlink::Get().GetPacketManager().Subscribe<EntityHandleFetchRequestPacket>(
			[this](const EntityHandleFetchRequestPacket& p, const PacketManager::PacketInfo& info)
			{ OnEntityHandleFetchRequest(p, info); });
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
		if (snapshot.empty())
			continue;

		const NetworkIdentity selfId = NetworkCredentials::Get().GetID();
		if (!BoundLeaser::Get().HasBound())
			continue;
		BoundLeaser::Get().GetBound(
			[&](const IBounds& b)
			{
				for (const auto& [ID, pos] : snapshot)
				{
					// Skip entities already in an active transfer.
					if (TransferCoordinator::Get().IsEntityInTransfer(ID))
					{
						continue;
					}

					if (b.Contains(pos))
					{
						continue;
					}

					EntitiesNewlyOutOfBounds.push_back(ID);
				}
			});

		if (!EntitiesNewlyOutOfBounds.empty())
		{
			TransferCoordinator::Get().MarkEntitiesForTransfer(std::span(EntitiesNewlyOutOfBounds));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}
void EntityLedger::AddEntity(const AtlasEntity& e)
{
	_WriteLock(
		[&]()
		{
			entities.insert(std::make_pair(e.Entity_ID, e));
			if (e.IsClient)
			{
				clients.insert(std::make_pair(e.Client_ID, e.Entity_ID));
			}
		});
	GlobalEntityLedger::Get().DeclareEntityRecord(NetworkCredentials::Get().GetID().ID,
												  e.Entity_ID);
}
void EntityLedger::_EraseEntity(AtlasEntityID ID)
{
	const auto f = entities.find(ID);
	if (f->second.IsClient)
	{
		clients.erase(f->second.Client_ID);
	}
	entities.erase(f);
	GlobalEntityLedger::Get().DeclareEntityRecord(NetworkCredentials::Get().GetID().ID, ID);
}
void EntityLedger::OnEntityHandleFetchRequest(const EntityHandleFetchRequestPacket& packet,
											  const PacketManager::PacketInfo& info)
{
	logger.DebugFormatted(
		"Received EntityHandleFetchRequestPacket from {} in state {}", info.sender.ToString(),
		packet.currentState == EntityHandleFetchRequestPacket::State::eRequest ? "Request"
																			   : "Response");
	if (packet.currentState == EntityHandleFetchRequestPacket::State::eRequest)
	{
		// logger.DebugFormatted("Received EntityHandleFetchRequestPacket from {}",
		//					  info.sender.ToString());
		const auto& requestData = packet.GetRequestData();
		AtlasEntityID requestedID = requestData.entityID;
		logger.DebugFormatted("Getting Data for Entity {} ", UUIDGen::ToString(requestedID));
		_ReadLock(
			[&]()
			{
				if (ExistsEntity(requestedID))
				{
					logger.DebugFormatted("Entity {} exists", UUIDGen::ToString(requestedID));
					EntityHandleFetchRequestPacket responsePacket;
					responsePacket.currentState = EntityHandleFetchRequestPacket::State::eResponse;
					EntityHandleFetchRequestPacket::ResponseData responseData;
					responseData.entityData = GetEntity(requestedID);
					responsePacket.data = responseData;
					logger.DebugFormatted("Responding with data for entity {}",
										  UUIDGen::ToString(requestedID));
					Interlink::Get().SendMessage(info.sender, responsePacket,
												 NetworkMessageSendFlag::eReliableBatched);
				}
				else {
					logger.DebugFormatted("Entity {} Does not exist", UUIDGen::ToString(requestedID));
				
				}
			});
	}
}

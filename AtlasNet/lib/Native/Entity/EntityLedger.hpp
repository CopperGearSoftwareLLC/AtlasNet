#pragma once

#include <boost/container/flat_map.hpp>
#include <queue>
#include <stop_token>
#include <thread>
#include <unordered_set>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Packet/ClientTransferPacket.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "Network/Packet/PacketManager.hpp"
class EntityLedger : public Singleton<EntityLedger>
{
	boost::container::flat_map<AtlasEntityID, AtlasEntity> entities;

	std::unordered_set<AtlasEntityID> entitiesMarkedForTransfer;
	PacketManager::Subscription sub_EntityListRequestPacket, sub_EntityTransferPacket,
		sub_ClientTransferPacket;
	Log logger = Log("EntityLedger");
	std::jthread LoopThread;

   public:
	void Init();

	auto ViewLocalEntities()
	{
		return std::ranges::transform_view(entities,  // underlying range
										   [](auto& kv) -> AtlasEntity&
										   {  // projection lambda
											   return kv.second;
										   });
	}
	void RegisterNewEntity(const AtlasEntity& e)
	{
		ASSERT(!entities.contains(e.Entity_ID), "Duplicate Entities");
		entities.insert(std::make_pair(e.Entity_ID, e));
	}
	[[nodiscard]] bool IsEntityClient(AtlasEntityID ID) const
	{
		const auto it = entities.find(ID);
		ASSERT(it != entities.end(), "Invalid ID");
		return it->second.IsClient;
	}

   private:
	void OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
								  const PacketManager::PacketInfo& info);

	void LoopThreadEntry(std::stop_token st);

	void onClientTransferPacket(const ClientTransferPacket& packet,
								const PacketManager::PacketInfo& info);
	void onEntityTransferPacket(const EntityTransferPacket& packet,
								const PacketManager::PacketInfo& info);
};
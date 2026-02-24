#pragma once

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <mutex>
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
	PacketManager::Subscription sub_EntityListRequestPacket;
	Log logger = Log("EntityLedger");
	std::jthread LoopThread;
	mutable std::mutex EntityListMutex;

   private:
	const AtlasEntity& _GetEntity(AtlasEntityID ID) const { return entities.at(ID); }
	void _EraseEntity(AtlasEntityID ID) { entities.erase(ID); }
	bool _ExistsEntity(AtlasEntityID ID) const { return entities.contains(ID); }

   public:
	void Init();
	template <typename ExecutionPolicy, typename Func>
	void ForEachEntity(ExecutionPolicy&& policy, Func&& fn)
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);

		std::for_each(std::forward<ExecutionPolicy>(policy), entities.begin(), entities.end(),
					  [fn = fn](auto& e) { fn(e.second); });
	}
	void RegisterNewEntity(const AtlasEntity& e)
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);
		ASSERT(!entities.contains(e.Entity_ID), "Duplicate Entities");
		entities.insert(std::make_pair(e.Entity_ID, e));
	}
	[[nodiscard]] bool IsEntityClient(AtlasEntityID ID) const
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);
		const auto it = entities.find(ID);
		ASSERT(it != entities.end(), "Invalid ID");
		return it->second.IsClient;
	}
	[[nodiscard]] const AtlasEntity& GetEntity(AtlasEntityID ID) const
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);

		return _GetEntity(ID);
	}
	void EraseEntity(AtlasEntityID ID)
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);

		_EraseEntity(ID);
	}
	bool ExistsEntity(AtlasEntityID ID) const
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);
		return _ExistsEntity(ID);
	}
	[[nodiscard]] AtlasEntity GetAndEraseEntity(AtlasEntityID ID)
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);
		AtlasEntity e = _GetEntity(ID);
		_EraseEntity(ID);
		return e;
	}

	void AddEntity(const AtlasEntity& e)
	{
		std::lock_guard<std::mutex> lock(EntityListMutex);

		entities.insert(std::make_pair(e.Entity_ID, e));
	}

   private:
	void OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
								  const PacketManager::PacketInfo& info);

	void LoopThreadEntry(std::stop_token st);
};
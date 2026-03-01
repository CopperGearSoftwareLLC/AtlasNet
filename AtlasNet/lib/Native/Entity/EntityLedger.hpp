#pragma once

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "Client/Packet/ClientTransferPacket.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "Network/Packet/PacketManager.hpp"
class EntityLedger : public Singleton<EntityLedger>
{
	std::unordered_map<AtlasEntityID, AtlasEntity> entities;
	PacketManager::Subscription sub_EntityListRequestPacket;
	Log logger = Log("EntityLedger");
	std::jthread LoopThread;
	mutable std::shared_mutex EntityListMutex;

   private:
	const AtlasEntity& _GetEntity(AtlasEntityID ID) const { return entities.at(ID); }
	void _EraseEntity(AtlasEntityID ID) { entities.erase(ID); }
	bool _ExistsEntity(AtlasEntityID ID) const { return entities.contains(ID); }

   public:
	template <typename FN>
	auto _ReadLock(FN&& f) const
	{
		std::shared_lock lock(EntityListMutex);
		return std::forward<FN>(f)();
	}
	template <typename FN>
	auto _WriteLock(FN&& f)
	{
		std::unique_lock lock(EntityListMutex);
		return std::forward<FN>(f)();
	}
	void Init();
	template <typename ExecutionPolicy, typename Func>
	void ForEachEntityWrite(ExecutionPolicy&& policy, Func&& fn)
	{
		_WriteLock(
			[&]()
			{
				std::for_each(std::forward<ExecutionPolicy>(policy), entities.begin(),
							  entities.end(), [fn = fn](auto& e) { fn(e.second); });
			});
	}
	template <typename ExecutionPolicy, typename Func>
	void ForEachEntityRead(ExecutionPolicy&& policy, Func&& fn)
	{
		_ReadLock(
			[&]()
			{
				std::for_each(std::forward<ExecutionPolicy>(policy), entities.begin(),
							  entities.end(), [fn = fn](const auto& e) { fn(e.second); });
			});
	}
	[[nodiscard]] bool IsEntityClient(AtlasEntityID ID) const
	{
		return _ReadLock(
			[&]()
			{
				const auto it = entities.find(ID);
				ASSERT(it != entities.end(), "Invalid ID");
				return it->second.IsClient;
			});
	}
	[[nodiscard]] AtlasEntity GetEntity(AtlasEntityID ID) const
	{
		return _ReadLock([&]() { return _GetEntity(ID); });
	}
	template <typename FN>
	void GetEntity(AtlasEntityID ID, FN&& f) const
	{
		_ReadLock([&]() { std::forward<FN>(f)(_GetEntity(ID)); });
	}
	[[nodiscard]] AtlasEntityMinimal GetEntityMinimal(AtlasEntityID ID) const
	{
		return _ReadLock([&]() { return (AtlasEntityMinimal)_GetEntity(ID); });
	}
	void EraseEntity(AtlasEntityID ID)
	{
		_WriteLock([&]() { _EraseEntity(ID); });
	}
	bool ExistsEntity(AtlasEntityID ID) const
	{
		return _ReadLock([&]() { return _ExistsEntity(ID); });
	}
	[[nodiscard]] AtlasEntity GetAndEraseEntity(AtlasEntityID ID)
	{
		return _WriteLock(
			[&]()
			{
				AtlasEntity e = _GetEntity(ID);
				_EraseEntity(ID);
				return e;
			});
	}

	void AddEntity(const AtlasEntity& e)
	{
		_WriteLock([&]() { entities.insert(std::make_pair(e.Entity_ID, e)); });
	}

   private:
	void OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
								  const PacketManager::PacketInfo& info);

	void LoopThreadEntry(std::stop_token st);
};

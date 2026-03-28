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

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Packet/EntityHandleFetchRequestPacket.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "Network/Packet/PacketManager.hpp"
class EntityLedger : public Singleton<EntityLedger>
{
	std::unordered_map<AtlasEntityID, AtlasEntity> entities;
	std::unordered_map<ClientID, AtlasEntityID> clients;
	PacketManager::Subscription sub_EntityListRequestPacket,sub_EntityHandleFetchRequestPacket;
	Log logger = Log("EntityLedger");
	std::jthread LoopThread;
	mutable std::shared_mutex EntityListMutex;

   private:
	const AtlasEntity& _GetEntity(AtlasEntityID ID) const { return entities.at(ID); }
	AtlasEntity& _GetEntity(AtlasEntityID ID) { return entities.at(ID); }
	std::optional<AtlasEntityID> _GetClientEntityID(const ClientID& cid) const
	{
		return clients.find(cid) != clients.end() ? std::optional<AtlasEntityID>(clients.at(cid))
												  : std::nullopt;
	}
	void _EraseEntity(AtlasEntityID ID);
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
		requires std::is_invocable_v<Func, AtlasEntity&>
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
		requires std::is_invocable_v<Func, const AtlasEntity&>
	void ForEachEntityRead(ExecutionPolicy&& policy, Func&& fn)
	{
		_ReadLock(
			[&]()
			{
				std::for_each(std::forward<ExecutionPolicy>(policy), entities.begin(),
							  entities.end(), [fn = fn](const auto& e) { fn(e.second); });
			});
	}
	template <typename ExecutionPolicy, typename Func>
		requires std::is_invocable_v<Func, AtlasEntity&>
	void ForEachClientWrite(ExecutionPolicy&& policy, Func&& fn)
	{
		_WriteLock(
			[&]()
			{
				std::for_each(std::forward<ExecutionPolicy>(policy), clients.begin(), clients.end(),
							  [&](const std::pair<ClientID, AtlasEntityID>& entry)
							  { fn(entities.at(entry.second)); });
			});
	}
	template <typename ExecutionPolicy, typename Func>
		requires std::is_invocable_v<Func, const AtlasEntity&>
	void ForEachClientRead(ExecutionPolicy&& policy, Func&& fn)
	{
		_ReadLock(
			[&]()
			{
				std::for_each(std::forward<ExecutionPolicy>(policy), clients.begin(), clients.end(),
							  [&](const std::pair<ClientID, AtlasEntityID>& entry)
							  { fn(entities.at(entry.second)); });
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
	template <typename FN>
	void GetEntity(AtlasEntityID ID, FN&& f)
	{
		_WriteLock([&]() { std::forward<FN>(f)(_GetEntity(ID)); });
	}
	size_t GetEntityCount() const
	{
		return _ReadLock([&]() { return entities.size(); });
	}
	std::optional<AtlasEntityID> GetClientEntityID(const ClientID& cid) const
	{
		return _ReadLock([&]() { return _GetClientEntityID(cid); });
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
	[[nodiscard]] std::optional<AtlasEntity> GetAndEraseEntity(AtlasEntityID ID)
	{
		return _WriteLock(
			[&]()->std::optional<AtlasEntity>
			{
				if (!_ExistsEntity(ID))
					return std::nullopt;
				AtlasEntity e = _GetEntity(ID);
				_EraseEntity(ID);
				return std::make_optional(e);
			});
	}

	void AddEntity(const AtlasEntity& e);

   private:
	void OnLocalEntityListRequest(const LocalEntityListRequestPacket& p,
								  const PacketManager::PacketInfo& info);

	void OnEntityHandleFetchRequest(const EntityHandleFetchRequestPacket& packet,
									const PacketManager::PacketInfo& info);
	void LoopThreadEntry(std::stop_token st);
};

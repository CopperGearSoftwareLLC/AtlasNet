#pragma once

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <chrono>
#include <execution>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "Transfer/Packet/ClientSwitchPacket.hpp"
#include "Transfer/Packet/ClientTransferPacket.hpp"
#include "Transfer/Packet/EntityTransferPacket.hpp"
#include "Transfer/TransferData.hpp"
#include "Transfer/TransferManifest.hpp"
class TransferCoordinator : public Singleton<TransferCoordinator>
{
	std::unordered_map<TransferID, EntityTransferData> EntityTransfers;
	std::stack<EntityTransferData> EntityPreTransfers;
	std::unordered_map<AtlasEntityID, TransferID> EntitiesInTransfer;

	std::unordered_map<ClientID, TransferID> ClientsInTransfer;
	std::stack<ClientTransferData> ClientsPreTransfers;
	std::unordered_map<TransferID, ClientTransferData> ClientTransfers;

	std::stack<AtlasEntityID> EntitiesToParseForReceiver;
	mutable std::shared_mutex transferMutex;
	std::jthread TransferThread;
	Log logger = Log("TransferCoordinator");
	PacketManager::Subscription EntityTransferPacketSubscription, ClientTransferPacketSubscription,
		ClientSwitchRequestPacketSubscription;

	template <typename FN>
	decltype(auto) _ReadLock(FN&& fn) const
	{
		std::shared_lock lock(transferMutex);
		return std::invoke(std::forward<FN>(fn));
	}

	template <typename FN>
	decltype(auto) _WriteLock(FN&& fn)
	{
		std::unique_lock lock(transferMutex);
		return std::invoke(std::forward<FN>(fn));
	}

   public:
	TransferCoordinator();
	void MarkEntitiesForTransfer(const std::span<AtlasEntityID> entities);
	bool IsEntityInTransfer(AtlasEntityID ID) const;

   private:
	void TransferThreadEntry(std::stop_token st);

	void ParseEntitiesForTargets();
	void TransferTick();
	void OnEntityTransferPacketArrival(const EntityTransferPacket& p,
									   const PacketManager::PacketInfo& info);
	void OnClientTransferPacketArrival(const ClientTransferPacket& p,
									   const PacketManager::PacketInfo& info);
	void OnClientSwitchPacketArrival(const ClientSwitchPacket& p,
									 const PacketManager::PacketInfo& info);
};
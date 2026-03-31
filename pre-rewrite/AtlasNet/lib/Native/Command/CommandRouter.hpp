#pragma once

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <shared_mutex>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Client/Database/ClientManifest.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "Transfer/Packet/ClientSwitchPacket.hpp"
#include "Transfer/TransferData.hpp"
class CommandRouter : public Singleton<CommandRouter>
{
	PacketManager::Subscription ServerStatePacketSub, ClientIntentPacketSub, ClientSwitchPacketSub;
	Log logger = Log("CommandRouter");

	std::unordered_map<ClientID, ShardID> RoutingMap;
	std::unordered_set<ClientID> RoutesPaused;
	std::unordered_map<TransferID,std::unordered_set<ClientID>> transferMap;
	mutable std::shared_mutex mutex;

   private:
	template <typename FN>
	auto _ReadLock(FN&& f) const
	{
		std::shared_lock lock(mutex);
		return std::forward<FN>(f)();
	}
	template <typename FN>
	auto _WriteLock(FN&& f)
	{
		std::unique_lock lock(mutex);
		return std::forward<FN>(f)();
	}

   public:
	CommandRouter();

	void OnServerStatePacket(const ServerStateCommandPacket& packet,
							 const PacketManager::PacketInfo& info);
	void OnClientIntentPacket(const ClientIntentCommandPacket& packet,
							  const PacketManager::PacketInfo& info);
	void OnClientSwitchPacket(const ClientSwitchPacket& packet,
							  const PacketManager::PacketInfo& info);
};
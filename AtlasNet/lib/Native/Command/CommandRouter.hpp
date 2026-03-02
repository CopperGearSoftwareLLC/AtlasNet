#pragma once

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <shared_mutex>

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
class CommandRouter : public Singleton<CommandRouter>
{
	PacketManager::Subscription ServerStatePacketSub, ClientIntentPacketSub;
	Log logger = Log("CommandRouter");

	std::unordered_map<ClientID, ShardID> RoutingMap;
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
	CommandRouter()
	{
		ServerStatePacketSub =
			Interlink::Get().GetPacketManager().Subscribe<ServerStateCommandPacket>(
				[this](const ServerStateCommandPacket& packet,
					   const PacketManager::PacketInfo& info)
				{ OnServerStatePacket(packet, info); });

		ClientIntentPacketSub =
			Interlink::Get().GetPacketManager().Subscribe<ClientIntentCommandPacket>(
				[this](const ClientIntentCommandPacket& packet,
					   const PacketManager::PacketInfo& info)
				{ OnClientIntentPacket(packet, info); });
	};

	void OnServerStatePacket(const ServerStateCommandPacket& packet,
							 const PacketManager::PacketInfo& info)
	{
		NetworkIdentity TargetID = NetworkIdentity::MakeIDGameClient(packet.target);
		Interlink::Get().SendMessage(TargetID, packet, NetworkMessageSendFlag::eReliableBatched);

		logger.DebugFormatted(
			"Dispatching ServerState\n - ID: {}\n - From Shard: {}\n - To Client: {}",
			packet.cmdTypeID, UUIDGen::ToString(info.sender.ID), UUIDGen::ToString(packet.target));
	};
	void OnClientIntentPacket(const ClientIntentCommandPacket& packet,
							  const PacketManager::PacketInfo& info)
	{
		auto SendPacketFunc = [&](const ShardID& shardID, const ClientIntentCommandPacket& packet)
		{
			logger.DebugFormatted(
				"Dispatching ClientIntent\n - ID: {}\n - From Client: {}\n - To Shard: {}",
				packet.cmdTypeID, UUIDGen::ToString(info.sender.ID), UUIDGen::ToString(shardID));
			Interlink::Get().SendMessage(NetworkIdentity::MakeIDShard(shardID), packet,
										 NetworkMessageSendFlag::eReliableBatched);
		};
		const bool success = _ReadLock(
			[&]()
			{
				if (!RoutingMap.contains(info.sender.ID))
				{
					return false;
				}

				SendPacketFunc(RoutingMap.at(info.sender.ID), packet);
				return true;
			});
		if (!success)
		{
			const std::optional<NetworkIdentity> Shard =
				ClientManifest::Get().GetClientShard(info.sender.ID);
			ASSERT(Shard.has_value(), "INVALID");
			_WriteLock([&]() { RoutingMap.insert(std::make_pair(info.sender.ID, Shard->ID)); });
			SendPacketFunc(Shard->ID, packet);
		}

		// logger.DebugFormatted("Received ClientIntentPacket");
	};
};
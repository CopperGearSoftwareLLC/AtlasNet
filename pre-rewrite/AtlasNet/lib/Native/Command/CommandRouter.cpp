#include "CommandRouter.hpp"

#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Transfer/Packet/ClientSwitchPacket.hpp"
#include "Transfer/TransferData.hpp"
void CommandRouter::OnClientIntentPacket(const ClientIntentCommandPacket& packet,
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

	bool RoutePaused;
	const bool success = _ReadLock(
		[&]()
		{
			if (RoutesPaused.contains(info.sender.ID))
			{
				RoutePaused = true;
				return false;
			}
			else
			{
				RoutePaused = false;
			}
			if (!RoutingMap.contains(info.sender.ID))
			{
				return false;
			}

			SendPacketFunc(RoutingMap.at(info.sender.ID), packet);
			return true;
		});
	if (RoutePaused)
	{
		return;
	}
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
void CommandRouter::OnServerStatePacket(const ServerStateCommandPacket& packet,
										const PacketManager::PacketInfo& info)
{
	const bool paused = _ReadLock([&]() { return (RoutesPaused.contains(packet.target)); });
	if (paused)
	{
		logger.ErrorFormatted("Packet dropped!!!!!");
		//return;
	}
	NetworkIdentity TargetID = NetworkIdentity::MakeIDGameClient(packet.target);
	Interlink::Get().SendMessage(TargetID, packet, NetworkMessageSendFlag::eReliableBatched);
	logger.DebugFormatted("Sending");
	logger.DebugFormatted("Dispatching ServerState\n - ID: {}\n - From Shard: {}\n - To Client: {}",
						  packet.cmdTypeID, UUIDGen::ToString(info.sender.ID),
						  UUIDGen::ToString(packet.target));
	logger.DebugFormatted("Done");

};
CommandRouter::CommandRouter()
{
	ServerStatePacketSub = Interlink::Get().GetPacketManager().Subscribe<ServerStateCommandPacket>(
		[this](const ServerStateCommandPacket& packet, const PacketManager::PacketInfo& info)
		{ OnServerStatePacket(packet, info); });

	ClientIntentPacketSub =
		Interlink::Get().GetPacketManager().Subscribe<ClientIntentCommandPacket>(
			[this](const ClientIntentCommandPacket& packet, const PacketManager::PacketInfo& info)
			{ OnClientIntentPacket(packet, info); });
	ClientSwitchPacketSub = Interlink::Get().GetPacketManager().Subscribe<ClientSwitchPacket>(
		[this](const ClientSwitchPacket& packet, const PacketManager::PacketInfo& info)
		{ OnClientSwitchPacket(packet, info); });
};
void CommandRouter::OnClientSwitchPacket(const ClientSwitchPacket& packet,
										 const PacketManager::PacketInfo& info)
{
	_WriteLock(
		[&]()
		{
			switch (packet.stage)
			{
				case ClientSwitchStage::eNone:
				case ClientSwitchStage::eFreeze:
					ASSERT(false, "This should never happen");
					break;

				case ClientSwitchStage::eRequestSwitch:
				{
					for (const auto& clientID : packet.GetAsRequestSwitchStage().clientIDs)
					{
						RoutesPaused.insert(clientID);
						transferMap[packet.TransferID].insert(clientID);
					}
					ClientSwitchPacket response;
					response.stage = ClientSwitchStage::eFreeze;
					response.TransferID = packet.TransferID;
					response.SetAsFreezeStage();

					Interlink::Get().SendMessage(info.sender, response,
												 NetworkMessageSendFlag::eReliableNow);
					logger.DebugFormatted("Froze routes for transfer {}",
										  UUIDGen::ToString(packet.TransferID));
				}
				break;
				case ClientSwitchStage::eActivate:

					for (const auto& clientID : transferMap.at(packet.TransferID))
					{
						RoutingMap[clientID] = info.sender.ID;
						RoutesPaused.erase(clientID);
					}
					transferMap.erase(packet.TransferID);
					logger.DebugFormatted("routes restores for transfer {} to {}",
										  UUIDGen::ToString(packet.TransferID),
										  info.sender.ToString());
					break;
			}
		});
}

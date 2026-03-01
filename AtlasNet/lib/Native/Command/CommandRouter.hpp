#pragma once

#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/Packet/PacketManager.hpp"
class CommandRouter : public Singleton<CommandRouter>
{
	PacketManager::Subscription ServerStatePacketSub, ClientIntentPacketSub;
	Log logger = Log("CommandRouter");

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
		logger.DebugFormatted("Received ServerStatePacket");
	};
	void OnClientIntentPacket(const ClientIntentCommandPacket& packet,
							  const PacketManager::PacketInfo& info)
	{
		logger.DebugFormatted("Received ClientIntentPacket");
	};
};
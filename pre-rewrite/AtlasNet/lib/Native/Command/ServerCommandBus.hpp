#pragma once

#include "Client/Client.hpp"
#include "Command/CommandBus.hpp"
#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Global/Misc/UUID.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/Packet/Packet.hpp"
#include "Network/Packet/PacketManager.hpp"
class ServerCommandBus : public ICommandBus<ClientID, NetClientIntentHeader, NetServerStateHeader>
{
	PacketManager::Subscription ClientIntentCommandPacket_Sub;
	boost::container::small_vector<ServerStateCommandPacket, 10> packets;
	Log logger = Log("ServerCommandBus");
	void implParseCommand(ClientID target, const INetCommand& command) override;
	void implFlushCommands() override;

   public:
	ServerCommandBus()
	{
		ClientIntentCommandPacket_Sub =
			Interlink::Get().GetPacketManager().Subscribe<ClientIntentCommandPacket>(
				[this](const ClientIntentCommandPacket& packet,
					   const PacketManager::PacketInfo& info)
				{ OnClientIntentCommandPacket(packet, info); });
	}

   private:
	void OnClientIntentCommandPacket(const ClientIntentCommandPacket& packet,
									 const PacketManager::PacketInfo& info)
	{
		const auto commandID = packet.cmdTypeID;
		/* logger.DebugFormatted("Received ClientIntentCommand ID: {}", commandID); */
		const auto command = CommandRegistry::Get().MakeFromID(commandID);
		ByteReader br(packet.commandData);
		command->Deserialize(br);
		NetClientIntentHeader header;
		header.clientID = packet.Sender;
		std::optional<AtlasEntityID> entityID =
			EntityLedger::Get().GetClientEntityID(header.clientID);
		if (!entityID)
		{
			logger.ErrorFormatted(
				"Received command from client {} which has no associated entity.It may have been "
				"transfered, Fix this!!! Ignoring.",
				UUIDGen::ToString(header.clientID));
			return;
		}
		header.entityID = *entityID;
		ExecCallback(header, *command);
	}
};
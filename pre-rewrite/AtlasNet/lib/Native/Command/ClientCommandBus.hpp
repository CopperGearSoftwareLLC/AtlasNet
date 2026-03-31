#pragma once

#include "Client/ClientLink.hpp"
#include "Command/CommandBus.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Debug/Log.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
class ClientCommandBus : ICommandBus<NetworkIdentity, NetServerStateHeader, NetClientIntentHeader>

{
	PacketManager::Subscription sub_ServerStateCommandPacket;
	Log logger = Log("ClientCommandBus");
	boost::container::small_vector<ClientIntentCommandPacket, 10> packets;

   private:
	void implParseCommand(NetworkIdentity target, const INetCommand& command) override;
	void implFlushCommands() override;
	void OnServerStateCommandPacket(const ServerStateCommandPacket& p,
									const PacketManager::PacketInfo& info)
	{
		const auto commandID = p.cmdTypeID;
		logger.DebugFormatted("Received ServerStateCommand ID: {}", commandID);
		const auto command = CommandRegistry::Get().MakeFromID(commandID);
		ByteReader br(p.commandData);
		command->Deserialize(br);
		NetServerStateHeader header;
		ExecCallback(header, *command);
	}

   public:
	ClientCommandBus()
	{
		sub_ServerStateCommandPacket =
			ClientLink::Get().GetPacketManager().Subscribe<ServerStateCommandPacket>(
				[this](const ServerStateCommandPacket& p, const PacketManager::PacketInfo& info)
				{ OnServerStateCommandPacket(p, info); });
	}
	~ClientCommandBus() override {}
	template <typename T>
	void Dispatch(const T& c)
	{
		NetworkIdentity id;
		ICommandBus::Dispatch(id, c);
	}
	using ICommandBus::Flush;
	using ICommandBus::Subscribe;
};
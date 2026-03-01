#include "ServerCommandBus.hpp"

#include <algorithm>

#include "Client/Database/ClientManifest.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkIdentity.hpp"

void ServerCommandBus::implParseCommand(ClientID target, const INetCommand& command)
{
	const auto ClientProxy = ClientManifest::Get().GetClientProxy(target);
	ASSERT(ClientProxy.has_value(), "this should never occur");

	ServerStateCommandPacket& packet = packets.emplace_back();
	packet.target = target;
	packet.ServerStateHeader = NetServerStateHeader{};
	packet.cmdTypeID = command.GetCommandID();
	ByteWriter commandDataWriter;
	command.Serialize(commandDataWriter);
	packet.commandData.assign(commandDataWriter.bytes().begin(), commandDataWriter.bytes().end());
}
void ServerCommandBus::implFlushCommands()
{
	std::for_each(
		packets.cbegin(), packets.cend(),
		[this](const ServerStateCommandPacket& p)
		{
			std::optional<NetworkIdentity> target = ClientManifest::Get().GetClientProxy(p.target);
			ASSERT(target.has_value(), "This should never happen");
			Interlink::Get().SendMessage(*target, p, NetworkMessageSendFlag::eReliableBatched);
		});
}

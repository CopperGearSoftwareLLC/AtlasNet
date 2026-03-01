#include "ClientCommandBus.hpp"

#include <algorithm>

#include "Client/ClientLink.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
void ClientCommandBus::implParseCommand(NetworkIdentity target, const INetCommand& command)
{
	ClientIntentCommandPacket& packet = packets.emplace_back();
	packet.cmdTypeID = command.GetCommandID();
	ByteWriter commandDataWriter;
	command.Serialize(commandDataWriter);
	packet.commandData.assign(commandDataWriter.bytes().begin(), commandDataWriter.bytes().end());
}
void ClientCommandBus::implFlushCommands()
{
	NetworkIdentity Proxy = ClientLink::Get().GetManagingProxy();
	std::for_each(
		packets.cbegin(), packets.cend(), [Proxy = Proxy](const ClientIntentCommandPacket& p)
		{ Interlink::Get().SendMessage(Proxy, p, NetworkMessageSendFlag::eReliableBatched); });
}

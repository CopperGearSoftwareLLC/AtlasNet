#include "ClientCommandBus.hpp"

#include <algorithm>

#include "Client/ClientCredentials.hpp"
#include "Client/ClientLink.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
void ClientCommandBus::implParseCommand(NetworkIdentity target, const INetCommand& command)
{
	ClientIntentCommandPacket& packet = packets.emplace_back();
	packet.cmdTypeID = command.GetCommandID();
	packet.InsertCommand(command);
	packet.Sender = ClientCredentials::Get().GetClientID();
	logger.DebugFormatted("Dispatching ClientIntentCommand ID: {}", packet.cmdTypeID);
}
void ClientCommandBus::implFlushCommands()
{
	std::for_each(packets.cbegin(), packets.cend(), [](const ClientIntentCommandPacket& p)
				  { ClientLink::Get().SendMessage(p, NetworkMessageSendFlag::eReliableBatched); });
	packets.clear();
}

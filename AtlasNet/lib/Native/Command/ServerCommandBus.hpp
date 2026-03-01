#pragma once

#include "Client/Client.hpp"
#include "Command/CommandBus.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
class ServerCommandBus : public ICommandBus<ClientID, NetClientIntentHeader, NetServerStateHeader>
{
	boost::container::small_vector<ServerStateCommandPacket, 10> packets;
	void implParseCommand(ClientID target, const INetCommand& command) override;
	void implFlushCommands() override;
};
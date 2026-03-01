#pragma once

#include "Command/CommandBus.hpp"
#include "Command/NetCommand.hpp"
#include "Command/Packet/CommandPayloadPacket.hpp"
#include "Network/NetworkIdentity.hpp"
class ClientCommandBus : ICommandBus<NetworkIdentity, NetServerStateHeader, NetClientIntentHeader>

{
	boost::container::small_vector<ClientIntentCommandPacket,10> packets;
   private:
	void implParseCommand(NetworkIdentity target, const INetCommand& command) override;
	void implFlushCommands() override;

   public:
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
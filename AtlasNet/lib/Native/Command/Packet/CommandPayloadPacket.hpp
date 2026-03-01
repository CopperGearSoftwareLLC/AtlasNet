#pragma once

#include <boost/container/flat_map.hpp>
#include <type_traits>

#include "Client/Client.hpp"
#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/Packet/Packet.hpp"

/*
class IServerIntentCommandPacket
	: public TPacket<IServerIntentCommandPacket, "IServerIntentCommandPacket">
{
   public:
	ClientID target;
	NetServerStateHeader ServerStateHeader;

	void SerializeHeader(ByteWriter& bw) const
	{
		bw.uuid(target);
		ServerStateHeader.Serialize(bw);
	};
	void DeserializeHeader(ByteReader& br)
	{
		target = br.uuid();
		ServerStateHeader.Deserialize(br);
	}
	void SerializeData(ByteWriter& bw) const override
	{
		SerializeHeader(bw);
		SerializeCommand(bw);
	}
	void DeserializeData(ByteReader& br) override
	{
		DeserializeHeader(br);
		DeserializeCommand(br);
	}
	[[nodiscard]] bool ValidateData() const override { return true; }

	virtual void SerializeCommand(ByteWriter& bw) const = 0;
	virtual void DeserializeCommand(ByteReader& bw) = 0;
	[[nodiscard]] virtual CommandID GetCommandID() = 0;
};
ATLASNET_REGISTER_PACKET(IServerIntentCommandPacket);

template <typename CommandType>
	requires std::is_base_of_v<IServerStateCommand, CommandType>
class TServerIntentCommandPacket : IServerIntentCommandPacket
{
   public:
	CommandType type;
	void SerializeCommand(ByteWriter& bw) const { type.Serialize(bw); }
	void DeserializeCommand(ByteReader& bw) { type.Deserialize(bw); }
	CommandID GetCommandID() override { return CommandRegistry::Get().GetCommandID<CommandType>(); }
};
*/
class ServerStateCommandPacket
	: public TPacket<ServerStateCommandPacket, "ServerIntentCommandPacket">
{
   public:
	ClientID target;
	NetServerStateHeader ServerStateHeader;
	CommandID cmdTypeID;
	boost::container::small_vector<uint8_t, 64> commandData;

	void SerializeData(ByteWriter& bw) const override
	{
		bw.uuid(target);
		ServerStateHeader.Serialize(bw);
		bw.write_scalar<decltype(cmdTypeID)>(cmdTypeID);
		bw.blob(commandData);
	}
	void DeserializeData(ByteReader& br) override
	{
		target = br.uuid();
		ServerStateHeader.Deserialize(br);
		cmdTypeID = br.read_scalar<decltype(cmdTypeID)>();
		std::span blob = br.blob();
		commandData.assign(blob.begin(), blob.end());
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
	void InsertCommand(const IServerStateCommand& command)
	{
		ByteWriter bw;
		command.Serialize(bw);
		commandData.assign(bw.bytes().begin(), bw.bytes().end());
	}

	template <typename C>
		requires std::is_base_of_v<IServerStateCommand, C>
	C ExtractCommand() const
	{
		ByteReader br(commandData);
		C command;
		command.Deserialize(br);
		return command;
	}
};
ATLASNET_REGISTER_PACKET(ServerStateCommandPacket);

class ClientIntentCommandPacket
	: public TPacket<ClientIntentCommandPacket, "ClientIntentCommandPacket">
{
   public:
	CommandID cmdTypeID;
	boost::container::small_vector<uint8_t, 64> commandData;

	void SerializeData(ByteWriter& bw) const override { bw.blob(commandData); }
	void DeserializeData(ByteReader& br) override
	{
		std::span blob = br.blob();
		commandData.assign(blob.begin(), blob.end());
	}
	[[nodiscard]] bool ValidateData() const override { return true; }
	void InsertCommand(const IClientIntentCommand& command)
	{
		ByteWriter bw;
		command.Serialize(bw);
		commandData.assign(bw.bytes().begin(), bw.bytes().end());
	}

	template <typename C>
		requires std::is_base_of_v<IClientIntentCommand, C>
	C ExtractCommand() const
	{
		ByteReader br(commandData);
		C command;
		command.Deserialize(br);
		return command;
	}
};
ATLASNET_REGISTER_PACKET(ClientIntentCommandPacket);
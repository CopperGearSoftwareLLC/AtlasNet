#pragma once

#include "Packet/Packet.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

class CommandPacket : public TPacket<CommandPacket, PacketType::eCommand>
{
   public:
	enum class Type
	{
		eInvalidCommand,
		eFetchNewBounds,
		eShutdown,
	};
	Type command = Type::eInvalidCommand;
	CommandPacket() : TPacket(){}
	std::vector<uint8_t> Args;
	void SerializeData(ByteWriter& bw) const override
	{
		bw.write_scalar(command);
		bw.blob(std::span(Args));
	}
	void DeserializeData(ByteReader& br) override
	{
		command = br.read_scalar<decltype(command)>();
		const auto blob_span = br.blob();
		Args.assign(blob_span.begin(), blob_span.end());
	}

	CommandPacket& SetCommandType(Type t)
	{
		command = t;
		return *this;
	}
	CommandPacket& ReadArgs(std::function<void(ByteReader&)> lambda)
	{
		auto span = std::span(Args);
		ByteReader br(span);
		lambda(br);
		return *this;
	}
	CommandPacket& WriteArgs(std::function<void(ByteWriter&)> lambda)
	{
		ByteWriter br;
		lambda(br);
		const auto span = br.bytes();
		Args.assign(span.begin(), span.end());
		return *this;
	}
	bool ValidateData() const override {
        return command != CommandPacket::Type::eInvalidCommand;
    }
};

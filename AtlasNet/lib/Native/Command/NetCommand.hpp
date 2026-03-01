#pragma once

#include <cstdint>

#include "Client/Client.hpp"
#include "Entity/Entity.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/Types/FixedString.hpp"

using CommandID = uint64_t;
struct NetCommandHeader
{
	virtual void Serialize(ByteWriter& bw) const = 0;
	virtual void Deserialize(ByteReader& br) = 0;
};

struct NetClientIntentHeader : NetCommandHeader
{
	ClientID clientID;
	AtlasEntityID entityID;
	void Serialize(ByteWriter& bw) const
	{
		bw.uuid(clientID);
		bw.uuid(entityID);
	}
	void Deserialize(ByteReader& br)
	{
		clientID = br.uuid();
		entityID = br.uuid();
	}
};
struct NetServerStateHeader : NetCommandHeader
{
	uint64_t ServerTick;
	void Serialize(ByteWriter& bw) const { bw.write_scalar<decltype(ServerTick)>(ServerTick); }
	void Deserialize(ByteReader& br) { ServerTick = br.read_scalar<decltype(ServerTick)>(); }
};

class INetCommand
{
   public:
	virtual ~INetCommand() = default;
	virtual void Serialize(ByteWriter& bw) const = 0;
	virtual void Deserialize(ByteReader& br) = 0;
	virtual const std::string_view GetCommandName() const = 0;
	[[nodiscard]] virtual CommandID GetCommandID() const = 0;
};
class IClientIntentCommand : virtual public INetCommand
{
};
class IServerStateCommand : virtual public INetCommand
{
};
template <typename D, FixedString Name>
class TNetCommand : virtual public INetCommand
{
	static constexpr CommandID HashName(const std::string_view str)
	{
		const char* c = str.data();
		CommandID hash = 2166136261u;
		while (*c)
		{
			hash ^= static_cast<uint8_t>(*c++);
			hash *= 16777619u;
		}
		return hash;
	}

   protected:
	const static inline CommandID ID = HashName(Name.value);
	public:
	CommandID GetCommandID() const override { return ID; }
	[[nodiscard]] static CommandID GetCommandIDStatic() { return ID; }
	const std::string_view GetCommandName() const override { return Name.value; }
};

template <typename D, FixedString Name>
class TClientIntentCommand : virtual public TNetCommand<D, Name>, public IClientIntentCommand
{
	NetClientIntentHeader ClientIntentHeader;
};
template <typename D, FixedString Name>
class TServerStateCommand : virtual public TNetCommand<D, Name>, public IServerStateCommand
{
	NetServerStateHeader ServerStateHeader;

};

#define SERVER_STATE_COMMAND_BEGIN(Name) \
struct Name : public TServerStateCommand<Name, #Name> 
#define SERVER_STATE_COMMAND_END(Name) ATLASNET_REGISTER_COMMAND(Name); 

#define CLIENT_INTENT_COMMAND_BEGIN(Name) \
struct Name : public TClientIntentCommand<Name, #Name> 
#define CLIENT_INTENT_COMMAND_END(Name) ATLASNET_REGISTER_COMMAND(Name); 

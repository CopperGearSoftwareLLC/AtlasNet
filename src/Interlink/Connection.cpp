#include "Connection.hpp"

IPAddress::IPAddress()
{
	SteamAddress.Clear();
}

IPAddress::IPAddress(SteamNetworkingIPAddr addr)
{
	SteamAddress.Clear();
	SteamAddress = addr;
}

IPAddress IPAddress::MakeLocalHost(PortType port)
{
	IPAddress address;
	address.SetLocalHost(port);
	return address;
}
void IPAddress::SetLocalHost(PortType port)
{
	SetIPv4(127, 0, 0, 1, port);
	ASSERT(SteamAddress.IsLocalHost(), "Fuck?");
}

void IPAddress::SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port)
{
	SteamAddress.SetIPv4((a << 24) | (b << 16) | (c << 8) | d, port);
}

std::string IPAddress::ToString(bool IncludePort) const
{
	std::string str;
	str.resize(SteamAddress.k_cchMaxString);
	SteamAddress.ToString(str.data(), str.size(), IncludePort);
	return str;
}

SteamNetworkingIPAddr IPAddress::ToSteamIPAddr() const
{
	return SteamAddress;
}

void Connection::SetNewState(ConnectionState newState)
{
	oldState = state;
	state = newState;
}

std::string IdentityPacket::ToString() const
{
	std::ostringstream ss;
	ss << std::underlying_type_t<decltype(type)>(type);
	return ss.str();
}

IdentityPacket IdentityPacket::FromString(const std::string &str)
{
	IdentityPacket packet;
	std::istringstream ss(str);
	ss >> (std::underlying_type_t<decltype(type)> &)(packet.type);
	return packet;
}

void ServerIdentifier::SetGod()

{
	Type = InterlinkType::eGod;
	ID = 0;
}

void ServerIdentifier::SetPartition(uint32 _ID)

{
	Type = InterlinkType::ePartition;
	ID = _ID;
}

void ServerIdentifier::SetGameServer(uint32 _ID)
{
	Type = InterlinkType::eGameServer;
	ID = _ID;
}

void ServerIdentifier::SetGameClient(uint32 _ID)
{
	Type = InterlinkType::eGameClient;
	ID = _ID;
}

void ServerIdentifier::SetGodView()
{
	Type = InterlinkType::eGameClient;
	ID = 0;
}

ServerIdentifier ServerIdentifier::MakeIDGod()
{
    ServerIdentifier iden;
    iden.SetGod();
    return iden;
}

ServerIdentifier ServerIdentifier::MakeIDPartition(uint32 _ID)
{
    ServerIdentifier iden;
    iden.SetPartition(_ID);
    return iden;
}

ServerIdentifier ServerIdentifier::MakeIDGameServer(uint32 _ID)
{
    ServerIdentifier iden;
    iden.SetGameServer(_ID);
    return iden;
}

ServerIdentifier ServerIdentifier::MakeIDGameClient(uint32 _ID)
{
    ServerIdentifier iden;
    iden.SetGameClient(_ID);
    return iden;
}

ServerIdentifier ServerIdentifier::MakeIDGodView()
{
    ServerIdentifier iden;
    iden.SetGodView();
    return iden;
}

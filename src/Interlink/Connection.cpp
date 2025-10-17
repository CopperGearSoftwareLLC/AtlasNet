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

void IPAddress::SetIPv4(std::pair<std::array<uint8_t, 4>, PortType> IP)
{
	SetIPv4(IP.first[0],IP.first[1],IP.first[2],IP.first[3],IP.second);
}

std::pair<std::array<uint8_t, 4>, PortType> IPAddress::GetIPv4() const
{
    std::pair<std::array<uint8_t, 4>, PortType>r;
	const auto ip = SteamAddress.GetIPv4();
	r.first[0] = (ip >> 24) & 0xFF;
    r.first[1] = (ip >> 16) & 0xFF;
    r.first[2] = (ip >> 8)  & 0xFF;
	r.first[3] = ip & 0xFF;
	r.second = SteamAddress.m_port;
	return r;
}

std::string IPAddress::ToString(bool IncludePort) const
{
	std::string str;
	str.resize(SteamAddress.k_cchMaxString);
	SteamAddress.ToString(str.data(), str.size(), IncludePort);
	return str;
}

void IPAddress::Parse(const std::string &AddrStr)
{
	SteamAddress.ParseString(AddrStr.c_str());
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

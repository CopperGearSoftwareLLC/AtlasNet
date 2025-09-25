#pragma once
#include "InterlinkEnums.hpp"
#include "pch.hpp"
using PortType = uint16;
using IPv4_Type = uint32;
using IPv6_Type = std::array<uint8, 16>;
class IPAddress
{
	SteamNetworkingIPAddr SteamAddress;

  public:
	IPAddress()
	{
		SteamAddress.Clear();
	}
	IPAddress(SteamNetworkingIPAddr addr)
	{
		SteamAddress.Clear();
		SteamAddress = addr;
	}

	static IPAddress MakeLocalHost(PortType port)
	{
		IPAddress address;
		address.SetLocalHost(port);
		return address;
	};

	void SetLocalHost(PortType port)
	{
		SetIPv4(127, 0, 0, 1, port);
		ASSERT(SteamAddress.IsLocalHost(), "Fuck?");
	}
	void SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port)
	{
		SteamAddress.SetIPv4((a << 24) | (b << 16) | (c << 8) | d, port);
	}

	std::string ToString(bool IncludePort = true) const
	{
		std::string str;
		str.resize(SteamAddress.k_cchMaxString);
		SteamAddress.ToString(str.data(), str.size(), IncludePort);
		return str;
	}
	SteamNetworkingIPAddr ToSteamIPAddr() const
	{
		return SteamAddress;
	}
};

struct ConnectionProperties
{
	IPAddress address;
};
struct Connection
{
	IPAddress address;
	ConnectionState oldState, state;
	InterlinkType TargetType = InterlinkType::eInvalid;
	HSteamNetConnection Connection;

	void SetNewState(ConnectionState newState)
	{
		oldState = state;
		state = newState;
	}
};
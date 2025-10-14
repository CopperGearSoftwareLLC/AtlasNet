#pragma once
#include "InterlinkEnums.hpp"
#include "pch.hpp"
#include "InterlinkIdentifier.hpp"
using PortType = uint16;
using IPv4_Type = uint32;
using IPv6_Type = std::array<uint8, 16>;

class IPAddress
{
	SteamNetworkingIPAddr SteamAddress;

  public:
	IPAddress();
	IPAddress(SteamNetworkingIPAddr addr);

	static IPAddress MakeLocalHost(PortType port);
	void SetLocalHost(PortType port);
	void SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port);
	void SetIPv4(std::pair<std::array<uint8_t,4>,PortType> IP);
	std::pair<std::array<uint8_t,4>,PortType> GetIPv4() const;
	std::string ToString(bool IncludePort = true) const;
	void Parse(const std::string& AddrStr);
	SteamNetworkingIPAddr ToSteamIPAddr() const;
};

struct ConnectionProperties
{
	IPAddress address;
};
struct Connection
{
	IPAddress address;
	ConnectionState oldState, state;
	//InterlinkType TargetType = InterlinkType::eInvalid;
	InterLinkIdentifier target;
	HSteamNetConnection SteamConnection;
	
	uint64 MessagesSent;
	std::vector<std::pair<InterlinkMessageSendFlag,std::vector<std::byte>>> MessagesToSendOnConnect;

	void SetNewState(ConnectionState newState);
};


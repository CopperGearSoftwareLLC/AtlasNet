#pragma once
#include "GameNetworkingSockets.hpp"
#include "InterlinkEnums.hpp"
#include "pch.hpp"
#include "InterlinkIdentifier.hpp"
using PortType = uint16;
using IPv4_Type = uint32;
using IPv6_Type = std::array<uint8, 16>;
#include "Packet/Packet.hpp"
class IPAddress
{
	SteamNetworkingIPAddr SteamAddress;

public:
	IPAddress();
	IPAddress(SteamNetworkingIPAddr addr);

	static IPAddress MakeLocalHost(PortType port);
	static IPAddress FromString(const std::string_view str);
	void SetLocalHost(PortType port);
	void SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port);
	void SetIPv4(std::pair<std::array<uint8_t, 4>, PortType> IP);
	std::pair<std::array<uint8_t, 4>, PortType> GetIPv4() const;
	std::string ToString(bool IncludePort = true) const;
	void Parse(const std::string &AddrStr);
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
	// InterlinkType TargetType = InterlinkType::eInvalid;
	InterLinkIdentifier target;
	HSteamNetConnection SteamConnection;
  
  ConnectionKind kind = ConnectionKind::eInternal;
  bool IsExternal() const noexcept { return kind == ConnectionKind::eExternal; }

	uint64 MessagesSent;
	std::vector<std::pair<InterlinkMessageSendFlag, std::shared_ptr<IPacket>>> MessagesToSendOnConnect;

	void SetNewState(ConnectionState newState);
};

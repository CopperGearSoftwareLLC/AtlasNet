#pragma once
#include "InterlinkEnums.hpp"
#include "pch.hpp"
using PortType = uint16;
using IPv4_Type = uint32;
using IPv6_Type = std::array<uint8, 16>;

class ServerIdentifier
{
	InterlinkType Type = InterlinkType::eInvalid;
	uint32 ID = -1;

  public:
	ServerIdentifier() = default;
	void SetGod();
	void SetPartition(uint32 _ID);
	void SetGameServer(uint32 _ID);
	void SetGameClient(uint32 _ID);
	void SetGodView();
	static ServerIdentifier MakeIDGod();
	static ServerIdentifier MakeIDPartition(uint32 _ID);
	static ServerIdentifier MakeIDGameServer(uint32 _ID);
	static ServerIdentifier MakeIDGameClient(uint32 _ID);
	static ServerIdentifier MakeIDGodView();
};
class IPAddress
{
	SteamNetworkingIPAddr SteamAddress;

  public:
	IPAddress();
	IPAddress(SteamNetworkingIPAddr addr);

	static IPAddress MakeLocalHost(PortType port);
	void SetLocalHost(PortType port);
	void SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port);

	std::string ToString(bool IncludePort = true) const;
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
	InterlinkType TargetType = InterlinkType::eInvalid;
	HSteamNetConnection Connection;

	void SetNewState(ConnectionState newState);
};

struct IdentityPacket
{
	InterlinkType type;

	std::string ToString() const;
	static IdentityPacket FromString(const std::string &str);
};
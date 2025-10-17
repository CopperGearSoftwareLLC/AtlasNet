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

	uint64 MessagesSent;
	std::vector<std::pair<InterlinkMessageSendFlag, std::vector<std::byte>>> MessagesToSendOnConnect;

	void SetNewState(ConnectionState newState);

	std::string ToString() const
	{
		std::ostringstream oss;
		oss << "Connection {\n";
		oss << "  Address: " << address.ToString() << "\n";
		oss << "  OldState: " << static_cast<int>(oldState)
			<< " -> State: " << static_cast<int>(state) << "\n";
		oss << "  Target: \"" << target.ToString() << "\" ";
		for (const auto& c : target.ToString())
		{
			oss << (int)c << " ";

		}
		oss << std::endl;
		oss << "  SteamConnection: " << SteamConnection << "\n";
		oss << "  MessagesSent: " << MessagesSent << "\n";
		oss << "  MessagesToSendOnConnect: [\n";
		for (const auto &msgPair : MessagesToSendOnConnect)
		{
			const auto &flag = msgPair.first;
			const auto &data = msgPair.second;

			oss << "    { Flag=" << static_cast<int>(flag)
				<< ", Size=" << data.size() << " bytes, Data=[";
			size_t preview = std::min<size_t>(data.size(), 8);
			for (size_t i = 0; i < preview; ++i)
			{
				oss << std::hex << std::setw(2) << std::setfill('0')
					<< static_cast<int>(data[i]);
				if (i + 1 < preview)
					oss << " ";
			}
			if (data.size() > preview)
				oss << " ...";
			oss << "] }\n";
		}
		oss << "  ]\n";
		oss << "}";
		return oss.str();
	}
};

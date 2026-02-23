#pragma once
#include <Global/pch.hpp>
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include <steam/steamnetworkingtypes.h>
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
	static IPAddress FromString(const std::string_view str);
	void SetLocalHost(PortType port);
	void SetIPv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, PortType port);
	void SetIPv4(std::pair<std::array<uint8_t, 4>, PortType> IP);
	std::pair<std::array<uint8_t, 4>, PortType> GetIPv4() const;
	std::string ToString(bool IncludePort = true) const;
	void Parse(const std::string &AddrStr);
	SteamNetworkingIPAddr ToSteamIPAddr() const;

	void Serialize(ByteWriter& bw) const
	{
		bool isIPv4 = SteamAddress.IsIPv4();
		bw.u8(isIPv4);
		bw.u16(SteamAddress.m_port);

		if (isIPv4)
		{
			bw.u32(SteamAddress.GetIPv4());
		}
		else
		{
			ASSERT(false, "not implemented");
		}

	}
	void Deserialize(ByteReader& br)
	{
		bool isIPv4 = br.u8();
		uint16 port = br.u16();
		if (isIPv4)
		{
			SteamAddress.SetIPv4(br.u32(), port);
		}
		else {
			ASSERT(false, "not implemented");
		
		}
	}
};
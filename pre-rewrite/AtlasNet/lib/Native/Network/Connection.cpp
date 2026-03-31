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
//IPAddress IPAddress::FromString(const std::string_view str)
//{
//	IPAddress addr;
//	addr.SteamAddress.SetIPv4(uint32 nIP, uint16 nPort)
//	addr.SteamAddress.ParseString(str.data());
//    return addr;
//}
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
    uint32_t ip = SteamAddress.GetIPv4();
    uint16_t port = SteamAddress.m_port;

    // If stored in network order:
    // ip = ntohl(ip);
    // port = ntohs(port);

    uint8_t a = (ip >> 24) & 0xFF;
    uint8_t b = (ip >> 16) & 0xFF;
    uint8_t c = (ip >> 8)  & 0xFF;
    uint8_t d = (ip)       & 0xFF;

    std::array<char, 32> buffer; // enough for "255.255.255.255:65535"
    char* ptr = buffer.data();
    char* end = buffer.data() + buffer.size();

    auto write_uint = [&](uint32_t value)
    {
        auto result = std::to_chars(ptr, end, value);
        ptr = result.ptr;
    };

    write_uint(a); *ptr++ = '.';
    write_uint(b); *ptr++ = '.';
    write_uint(c); *ptr++ = '.';
    write_uint(d);

    if (IncludePort)
    {
        *ptr++ = ':';
        write_uint(port);
    }

    return std::string(buffer.data(), ptr - buffer.data());
}


void IPAddress::Parse(const std::string& AddrStr)
{
    std::string_view str{ AddrStr };

    uint32_t parts[4] = {};
    uint16_t port = 0;

    const char* begin = str.data();
    const char* end   = begin + str.size();

    // Parse IPv4
    for (int i = 0; i < 4; ++i)
    {
        auto result = std::from_chars(begin, end, parts[i]);
        if (result.ec != std::errc{} || parts[i] > 255)
        {
            SteamAddress.Clear();
            return;
        }

        begin = result.ptr;

        if (i < 3)
        {
            if (begin >= end || *begin != '.')
            {
                SteamAddress.Clear();
                return;
            }
            ++begin;
        }
    }

    // Optional port
    if (begin < end && *begin == ':')
    {
        ++begin;

        auto portResult = std::from_chars(begin, end, port);
        if (portResult.ec != std::errc{} || port > 65535)
        {
            SteamAddress.Clear();
            return;
        }
    }

    // Pack IPv4 (network byte order)
    uint32_t ip =
        (parts[0] << 24) |
        (parts[1] << 16) |
        (parts[2] << 8)  |
        (parts[3]);

    // If SteamAddress expects network order:
    // ip = htonl(ip);
    // port = htons(port);

    SteamAddress.SetIPv4(ip,port);

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

#pragma once

#include "Address.hpp"
#include <cstdint>
#include <string>
using PortType = uint16_t;

class IEndPointAddress
{
  PortType port;

public:
  virtual ~IEndPointAddress() = default;
  PortType get_port() const
  {
    return port;
  }
  void set_port(PortType p)
  {
    port = p;
  }
  virtual std::string to_string() const = 0;
  virtual void parse_string(const std::string& str) = 0;
  virtual std::size_t hash() const noexcept = 0;
};

template <typename T>
  requires std::derived_from<T, IAddress>
class TEndPointAddress : public IEndPointAddress
{
  T address;

public:
  std::string to_string() const override
  {
    return address.to_string();
  }

  void parse_string(const std::string& str) override
  {
    address.parse_string(str);
    // now parse port
    size_t colon = str.rfind(':');
    if (colon == std::string::npos)
      throw std::invalid_argument("Invalid EndPointAddress: " + str);
    std::string port_str = str.substr(colon + 1);
    PortType port = static_cast<PortType>(std::stoul(port_str));
    set_port(port);
  }

  // Call .hash() directly — avoids needing std::hash<T> to be specialized yet
  std::size_t hash() const noexcept override
  {
    std::size_t h = address.hash();
    h ^= std::hash<PortType>{}(get_port()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
    return h;
  }

  T get_address() const
  {
    return address;
  }
  void set_address(const T& addr)
  {
    address = addr;
  }
};

class EndPointAddress : public IEndPointAddress
{
  std::variant<std::monostate, IPv4Address, IPv6Address, DNSAddress,
               SteamIDAddress>
      address;

public:
  EndPointAddress(const std::string& str)
  {
    parse_string(str);
  }
  EndPointAddress(const IPv4Address& ipv4, PortType port) : address(ipv4)
  {
    set_port(port);
  }
  EndPointAddress(const IPv6Address& ipv6, PortType port) : address(ipv6)
  {
    set_port(port);
  }
  EndPointAddress(const DNSAddress& dns, PortType port) : address(dns)
  {
    set_port(port);
  }
  EndPointAddress(const SteamIDAddress& steamID, PortType port)
      : address(steamID)
  {
    set_port(port);
  }
  EndPointAddress(const SteamNetworkingIPAddr& steamAddr)
  {
    if (steamAddr.m_port == 0)
      throw std::invalid_argument(
          "Port must be specified in SteamNetworkingIPAddr");
    set_port(steamAddr.m_port);
    if (steamAddr.IsIPv4())
    {
      uint32_t ipv4_packed = steamAddr.GetIPv4();
      IPv4Address ipv4(static_cast<uint8_t>((ipv4_packed >> 24) & 0xFF),
                       static_cast<uint8_t>((ipv4_packed >> 16) & 0xFF),
                       static_cast<uint8_t>((ipv4_packed >> 8) & 0xFF),
                       static_cast<uint8_t>(ipv4_packed & 0xFF));
      address = ipv4;
    }
    else
    {
      const uint8_t* ipv6_bytes = steamAddr.m_ipv6;

      IPv6Address ipv6((static_cast<uint16_t>(ipv6_bytes[0]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[1]),
                       (static_cast<uint16_t>(ipv6_bytes[2]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[3]),
                       (static_cast<uint16_t>(ipv6_bytes[4]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[5]),
                       (static_cast<uint16_t>(ipv6_bytes[6]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[7]),
                       (static_cast<uint16_t>(ipv6_bytes[8]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[9]),
                       (static_cast<uint16_t>(ipv6_bytes[10]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[11]),
                       (static_cast<uint16_t>(ipv6_bytes[12]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[13]),
                       (static_cast<uint16_t>(ipv6_bytes[14]) << 8) |
                           static_cast<uint16_t>(ipv6_bytes[15]));
      address = ipv6;
    }
  }

  bool IsIpv4() const
  {
    return std::holds_alternative<IPv4Address>(address);
  }
  bool IsIpv6() const
  {
    return std::holds_alternative<IPv6Address>(address);
  }
  bool IsDNS() const
  {
    return std::holds_alternative<DNSAddress>(address);
  }
  bool IsSteamID() const
  {
    return std::holds_alternative<SteamIDAddress>(address);
  }

  SteamNetworkingIPAddr ToSteamAddr() const
  {
    SteamNetworkingIPAddr steamAddr;

    auto FromIpv4 = [](const IPv4Address& ipv4,
                       PortType port) -> SteamNetworkingIPAddr
    {
      SteamNetworkingIPAddr addr;
      uint32_t ipv4_packed = (static_cast<uint32_t>(ipv4[0]) << 24) |
                             (static_cast<uint32_t>(ipv4[1]) << 16) |
                             (static_cast<uint32_t>(ipv4[2]) << 8) |
                             static_cast<uint32_t>(ipv4[3]);
      addr.SetIPv4(ipv4_packed, port);
      return addr;
    };
    auto FromIpv6 = [](const IPv6Address& ipv6,
                       PortType port) -> SteamNetworkingIPAddr
    {
      SteamNetworkingIPAddr addr;
      uint8_t ipv6_bytes[16];
      for (size_t i = 0; i < 16; ++i)
      {
        ipv6_bytes[i] = ipv6[i];
      }
      addr.SetIPv6(ipv6_bytes, port);
      return addr;
    };

    /*  bool success = steamAddr.ParseString(to_string().c_str());
       if (!success)
       {
         throw std::runtime_error("Failed to parse EndPointAddress to
       SteamNetworkingIPAddr");
       } */
    if (IsIpv4())
    {
      const IPv4Address& ipv4 = std::get<IPv4Address>(address);
      steamAddr = FromIpv4(ipv4, get_port());
    }
    else if (IsIpv6())
    {
      const IPv6Address& ipv6 = std::get<IPv6Address>(address);
      steamAddr = FromIpv6(ipv6, get_port());
    }
    else if (IsDNS())
    {
      const DNSAddress& dns = std::get<DNSAddress>(address);
      auto resolved = dns.resolve();
      if (!resolved)
      {
        throw std::runtime_error("Failed to resolve DNS address: " +
                                 dns.get_hostname());
      }
      if (std::holds_alternative<IPv4Address>(*resolved))
      {
        const IPv4Address& ipv4 = std::get<IPv4Address>(*resolved);
        steamAddr = FromIpv4(ipv4, get_port());
      }
      else if (std::holds_alternative<IPv6Address>(*resolved))
      {
        const IPv6Address& ipv6 = std::get<IPv6Address>(*resolved);
        steamAddr = FromIpv6(ipv6, get_port());
      }
      else
      {
        throw std::runtime_error(
            "Resolved DNS address is neither IPv4 nor IPv6");
      }
    }
    else if (IsSteamID())
    {
      throw std::runtime_error("Invalid EndPointAddress variant");
    }
    else
    {
      throw std::runtime_error("Invalid EndPointAddress variant");
    }
    return steamAddr;
  }
  std::string to_string() const override
  {
    const std::string addrStr = std::visit(
        [](const auto& addr) -> std::string
        {
          using T = std::decay_t<decltype(addr)>;
          if constexpr (std::is_same_v<T, std::monostate>)
          {
            throw std::runtime_error("EndPointAddress is not initialized");
          }
          else
          {
            return addr.to_string();
          }
        },
        address);

    return addrStr + ":" + std::to_string(get_port());
  }
  void parse_string(const std::string& str) override
  {
    // Try parsing as IPv4 first, then IPv6, then SteamID.
    try
    {
      IPv4Address ipv4(str);
      address = ipv4;
    }
    catch (const std::exception&)
    {
      try
      {
        IPv6Address ipv6(str);
        address = ipv6;
      }
      catch (const std::exception&)
      {
        try
        {
          DNSAddress dns(str);
          address = dns;
        }
        catch (const std::exception&)
        {
          try
          {
            std::runtime_error(
                "Parsing SteamIDAddress from string not implemented");
          }
          catch (const std::exception&)
          {
            throw std::invalid_argument("Invalid EndPointAddress: " + str);
          }
        }
      }
    }

    // then after whichever succedded, parse port from end of string (after last
    // colon)

    size_t colon = str.rfind(':');
    if (colon == std::string::npos)
      throw std::invalid_argument("Invalid EndPointAddress: " + str);
    std::string port_str = str.substr(colon + 1);
    PortType port = static_cast<PortType>(std::stoul(port_str));
    set_port(port);
  }

  template <typename T>
    requires std::derived_from<T, IAddress>
  T get_address() const
  {
    if (auto ptr = std::get_if<T>(&address))
      return *ptr;
    throw std::runtime_error("Address type mismatch");
  }

  template <typename T>
    requires std::derived_from<T, IAddress>
  void set_address(const T& addr)
  {
    address = addr;
  }
  bool operator==(EndPointAddress other) const
  {
    if (get_port() != other.get_port())
      return false;

    if (address.index() != other.address.index())
      return false;

    if (const auto* a = std::get_if<IPv4Address>(&address))
    {
      const auto& b = std::get<IPv4Address>(other.address);
      for (size_t i = 0; i < 4; ++i)
      {
        if ((*a)[i] != b[i])
          return false;
      }
      return true;
    }

    if (const auto* a = std::get_if<IPv6Address>(&address))
    {
      const auto& b = std::get<IPv6Address>(other.address);
      for (size_t i = 0; i < 16; ++i)
      {
        if ((*a)[i] != b[i])
          return false;
      }
      return true;
    }

    if (const auto* a = std::get_if<SteamIDAddress>(&address))
    {
      const auto& b = std::get<SteamIDAddress>(other.address);
      return a->get_steam_id64() == b.get_steam_id64();
    }

    return false;
  }
  // Declared here, defined out-of-line AFTER std::hash specializations below
  std::size_t hash() const noexcept override;
};
// Now safe to define: all three specializations are already visible
inline std::size_t EndPointAddress::hash() const noexcept
{
  std::size_t h = std::visit(
      [](const auto& addr) -> std::size_t
      {
        using T = std::decay_t<decltype(addr)>;
        if constexpr (std::is_same_v<T, std::monostate>)
        {
          return 0u;
        }
        else
        {
          return std::hash<T>{}(addr);
        }
      },
      address);

  h ^= std::hash<PortType>{}(get_port()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
       (h >> 2);
  return h;
}

namespace std
{
template <> struct hash<EndPointAddress>
{
  std::size_t operator()(const EndPointAddress& a) const noexcept
  {
    return a.hash();
  }
};
} // namespace std

#pragma once

#include "Address.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
namespace AtlasNet
{

using PortType = uint16_t;

class ISocketAddress
{
  PortType port = 0;

public:
  virtual ~ISocketAddress() = default;

  PortType get_port() const
  {
    return port;
  }

  void set_port(PortType p)
  {
    if (p == 0)
      throw std::invalid_argument("Port number must be greater than 0");
    if (p > 65535)
      throw std::invalid_argument(
          "Port number must be less than or equal to 65535");
    port = p;
  }

  virtual std::string to_string() const = 0;
  virtual void parse_string(const std::string& str) = 0;
  virtual std::size_t hash() const noexcept = 0;
};
/* 
template <typename T>
  requires std::derived_from<T, IAddress>
class TSocketAddress : public ISocketAddress
{
  T address{};

public:
  TSocketAddress() = default;
  TSocketAddress(const T& addr, PortType port) : address(addr)
  {
    set_port(port);
  }

  std::string to_string() const override
  {
    if constexpr (std::is_same_v<T, IPv6>)
      return "[" + address.to_string() + "]:" + std::to_string(get_port());

    return address.to_string() + ":" + std::to_string(get_port());
  }

  void parse_string(const std::string& str) override
  {
    T parsed(str);
    address = parsed.template get_address<T>();
    set_port(parsed.get_port());
  }

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
}; */

class SocketAddress : public ISocketAddress
{
  std::variant<std::monostate, IPv4, IPv6, HostName, SteamIDAddress> address;

public:
  SocketAddress() = default;

  explicit SocketAddress(const std::string& str)
  {
    parse_string(str);
  }

  SocketAddress(const IPv4& ipv4, PortType port) : address(ipv4)
  {
    set_port(port);
  }

  SocketAddress(const IPv6& ipv6, PortType port) : address(ipv6)
  {
    set_port(port);
  }

  SocketAddress(const HostName& hostName, PortType port) : address(hostName)
  {
    set_port(port);
  }

  SocketAddress(const SteamIDAddress& steamID, PortType port) : address(steamID)
  {
    set_port(port);
  }

  explicit SocketAddress(const SteamNetworkingIPAddr& steamAddr)
  {
    if (steamAddr.m_port == 0)
      throw std::invalid_argument(
          "Port must be specified in SteamNetworkingIPAddr");

    set_port(steamAddr.m_port);

    if (steamAddr.IsIPv4())
    {
      const uint32_t ipv4_packed = steamAddr.GetIPv4();
      address = IPv4(static_cast<uint8_t>((ipv4_packed >> 24) & 0xFF),
                     static_cast<uint8_t>((ipv4_packed >> 16) & 0xFF),
                     static_cast<uint8_t>((ipv4_packed >> 8) & 0xFF),
                     static_cast<uint8_t>(ipv4_packed & 0xFF));
    }
    else
    {
      const uint8_t* ipv6_bytes = steamAddr.m_ipv6;
      address = IPv6((static_cast<uint16_t>(ipv6_bytes[0]) << 8) |
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
    }
  }

  bool IsIPv4() const
  {
    return std::holds_alternative<IPv4>(address);
  }
  bool IsIPv6() const
  {
    return std::holds_alternative<IPv6>(address);
  }
  bool IsHostName() const
  {
    return std::holds_alternative<HostName>(address);
  }
  bool IsSteamID() const
  {
    return std::holds_alternative<SteamIDAddress>(address);
  }
  bool IsValid() const
  {
    return !std::holds_alternative<std::monostate>(address);
  }

  const IPv4& get_ipv4() const
  {
    if (!IsIPv4())
      throw std::bad_variant_access();
    return std::get<IPv4>(address);
  }

  const IPv6& get_ipv6() const
  {
    if (!IsIPv6())
      throw std::bad_variant_access();
    return std::get<IPv6>(address);
  }

  const HostName& get_hostname() const
  {
    if (!IsHostName())
      throw std::bad_variant_access();
    return std::get<HostName>(address);
  }

  const SteamIDAddress& get_steam_id() const
  {
    if (!IsSteamID())
      throw std::bad_variant_access();
    return std::get<SteamIDAddress>(address);
  }

  SteamNetworkingIPAddr ToSteamAddr() const
  {
    auto FromIPv4 = [](const IPv4& ipv4, PortType port) -> SteamNetworkingIPAddr
    {
      SteamNetworkingIPAddr addr;
      const uint32_t ipv4_packed = (static_cast<uint32_t>(ipv4[0]) << 24) |
                                   (static_cast<uint32_t>(ipv4[1]) << 16) |
                                   (static_cast<uint32_t>(ipv4[2]) << 8) |
                                   static_cast<uint32_t>(ipv4[3]);
      addr.SetIPv4(ipv4_packed, port);
      return addr;
    };

    auto FromIPv6 = [](const IPv6& ipv6, PortType port) -> SteamNetworkingIPAddr
    {
      SteamNetworkingIPAddr addr;
      uint8_t ipv6_bytes[16];
      for (size_t i = 0; i < 16; ++i)
        ipv6_bytes[i] = ipv6[i];
      addr.SetIPv6(ipv6_bytes, port);
      return addr;
    };

    if (IsIPv4())
      return FromIPv4(std::get<IPv4>(address), get_port());

    if (IsIPv6())
      return FromIPv6(std::get<IPv6>(address), get_port());

    if (IsHostName())
    {
      const HostName& hostName = std::get<HostName>(address);
      auto resolved = hostName.resolve();
      if (!resolved)
        throw std::runtime_error("Failed to resolve host name: " +
                                 hostName.get_hostname());

      if (std::holds_alternative<IPv4>(*resolved))
        return FromIPv4(std::get<IPv4>(*resolved), get_port());

      if (std::holds_alternative<IPv6>(*resolved))
        return FromIPv6(std::get<IPv6>(*resolved), get_port());
    }

    throw std::runtime_error("Invalid SocketAddress variant");
  }

  std::string to_string() const override
  {
    const std::string addrStr = std::visit(
        [](const auto& addr) -> std::string
        {
          using T = std::decay_t<decltype(addr)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            throw std::runtime_error("SocketAddress is not initialized");
          else
            return addr.to_string();
        },
        address);

    if (IsIPv6())
      return "[" + addrStr + "]:" + std::to_string(get_port());

    return addrStr + ":" + std::to_string(get_port());
  }

  void parse_string(const std::string& str) override
  {
    if (str.empty())
      throw std::invalid_argument("Invalid SocketAddress: empty string");

    std::string addrPart;
    std::string portPart;

    if (str.front() == '[')
    {
      const size_t rbracket = str.find(']');
      if (rbracket == std::string::npos || rbracket + 1 >= str.size() ||
          str[rbracket + 1] != ':')
      {
        throw std::invalid_argument(
            "Invalid SocketAddress: malformed IPv6 endpoint");
      }

      addrPart = str.substr(1, rbracket - 1);
      portPart = str.substr(rbracket + 2);
    }
    else
    {
      const size_t colon = str.rfind(':');
      if (colon == std::string::npos)
        throw std::invalid_argument("Invalid SocketAddress: missing port");

      addrPart = str.substr(0, colon);
      portPart = str.substr(colon + 1);
    }

    try
    {
      size_t port = std::stoul(portPart);
      if (port == 0 || port > 65535)
        throw std::out_of_range("Port number out of range");
      set_port(static_cast<PortType>(port));
    }
    catch (const std::exception&)
    {
      throw std::invalid_argument("Invalid SocketAddress: invalid port");
    }

    try
    {
      address = IPv4(addrPart);
      return;
    }
    catch (const std::exception&)
    {
    }

    try
    {
      address = IPv6(addrPart);
      return;
    }
    catch (const std::exception&)
    {
    }

    try
    {
      address = HostName(addrPart);
      return;
    }
    catch (const std::exception&)
    {
    }

    throw std::invalid_argument("Invalid SocketAddress: " + str);
  }

  template <typename T>
    requires std::derived_from<T, IAddress>
  T get_address() const
  {
    if (const auto* ptr = std::get_if<T>(&address))
      return *ptr;
    throw std::runtime_error("Address type mismatch");
  }

  template <typename T>
    requires std::derived_from<T, IAddress>
  void set_address(const T& addr)
  {
    address = addr;
  }

  bool operator==(const SocketAddress& other) const
  {
    if (get_port() != other.get_port())
      return false;

    if (address.index() != other.address.index())
      return false;

    if (const auto* a = std::get_if<IPv4>(&address))
      return *a == std::get<IPv4>(other.address);
    if (const auto* a = std::get_if<IPv6>(&address))
      return *a == std::get<IPv6>(other.address);
    if (const auto* a = std::get_if<HostName>(&address))
      return *a == std::get<HostName>(other.address);
    if (const auto* a = std::get_if<SteamIDAddress>(&address))
      return *a == std::get<SteamIDAddress>(other.address);

    return std::holds_alternative<std::monostate>(address) &&
           std::holds_alternative<std::monostate>(other.address);
  }

  std::size_t hash() const noexcept override
  {
    std::size_t h = std::visit(
        [](const auto& addr) -> std::size_t
        {
          using T = std::decay_t<decltype(addr)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            return 0u;
          else
            return std::hash<T>{}(addr);
        },
        address);

    h ^= std::hash<PortType>{}(get_port()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
    return h;
  }
};
} // namespace AtlasNet
namespace std
{
template <> struct hash<AtlasNet::SocketAddress>
{
  std::size_t operator()(const AtlasNet::SocketAddress& a) const noexcept
  {
    return a.hash();
  }
};
} // namespace std

#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include "steam/steamtypes.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <steam/steam_api_common.h>
#include <string>
#include <variant>

class IAddress
{
public:
  virtual ~IAddress() = default;
  virtual std::string to_string() const = 0;
  virtual void parse_string(const std::string& str) = 0;
  virtual std::size_t hash() const noexcept = 0;
};

class IPv4Address : public IAddress
{
  uint8_t octets[4];

public:
  IPv4Address(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
  {
    octets[0] = o1;
    octets[1] = o2;
    octets[2] = o3;
    octets[3] = o4;
  }
  explicit IPv4Address(const std::string& str)
  {
    parse_string(str);
  }
  void parse_string(const std::string& str) override
  {
    if ("localhost" == str)
    {
      octets[0] = 127;
      octets[1] = 0;
      octets[2] = 0;
      octets[3] = 1;
      return;
    }
    size_t start = 0;
    for (int i = 0; i < 4; ++i)
    {
      size_t dot = str.find('.', start);
      if (dot == std::string::npos && i < 3)
        throw std::invalid_argument("Invalid IPv4 address: " + str);
      std::string octet_str = str.substr(start, dot - start);
      int val = std::stoi(octet_str);
      if (val < 0 || val > 255)
        throw std::out_of_range("Octet out of range: " + octet_str);
      octets[i] = static_cast<uint8_t>(val);
      start = dot + 1;
    }
  }

  std::string to_string() const override
  {
    return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
           std::to_string(octets[2]) + "." + std::to_string(octets[3]);
  }

  std::size_t hash() const noexcept override
  {
    const uint32_t packed = (static_cast<uint32_t>(octets[0]) << 24) |
                            (static_cast<uint32_t>(octets[1]) << 16) |
                            (static_cast<uint32_t>(octets[2]) << 8) |
                            static_cast<uint32_t>(octets[3]);
    return std::hash<uint32_t>{}(packed);
  }

  uint8_t operator[](size_t index) const
  {
    if (index >= 4)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return octets[index];
  }
  uint8_t& operator[](size_t index)
  {
    if (index >= 4)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return octets[index];
  }
  size_t size() const
  {
    return 4;
  }
};

class IPv6Address : public IAddress
{
  uint16_t segments[8];

public:
  IPv6Address(uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5,
              uint16_t s6, uint16_t s7, uint16_t s8)
  {
    segments[0] = s1;
    segments[1] = s2;
    segments[2] = s3;
    segments[3] = s4;
    segments[4] = s5;
    segments[5] = s6;
    segments[6] = s7;
    segments[7] = s8;
  }
  explicit IPv6Address(const std::string& str)
  {
    parse_string(str);
  }
  void parse_string(const std::string& str) override
  {
    size_t start = 0;
    for (int i = 0; i < 8; ++i)
    {
      size_t colon = str.find(':', start);
      if (colon == std::string::npos && i < 7)
        throw std::invalid_argument("Invalid IPv6 address: " + str);
      std::string segment_str = str.substr(start, colon - start);
      uint16_t val =
          static_cast<uint16_t>(std::stoul(segment_str, nullptr, 16));
      segments[i] = val;
      start = colon + 1;
    }
  }

  std::string to_string() const override
  {
    std::string result;
    for (int i = 0; i < 8; ++i)
    {
      if (i > 0)
        result += ":";
      result += std::to_string(segments[i]);
    }
    return result;
  }

  std::size_t hash() const noexcept override
  {
    std::size_t h = 0;
    for (int i = 0; i < 8; ++i)
    {
      h ^= std::hash<uint16_t>{}(segments[i]) + 0x9e3779b97f4a7c15ULL +
           (h << 6) + (h >> 2);
    }
    return h;
  }

  uint8_t operator[](size_t index) const
  {
    if (index >= 16)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return reinterpret_cast<const uint8_t*>(segments)[index];
  }
  uint8_t& operator[](size_t index)
  {
    if (index >= 16)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return reinterpret_cast<uint8_t*>(segments)[index];
  }
  size_t size() const
  {
    return 16;
  }
};

class SteamIDAddress : public IAddress
{
  SteamNetworkingIdentity identity;

public:
  void parse_string(const std::string& str) override
  {
    throw std::runtime_error(
        "Parsing SteamIDAddress from string not implemented");
  }
  explicit SteamIDAddress(CSteamID steamID)
  {
    identity.SetSteamID(steamID);
  }
  explicit SteamIDAddress(uint64_t steamID)
  {
    identity.SetSteamID64(steamID);
  }

  std::string to_string() const override
  {
    if (CSteamID steamID = identity.GetSteamID(); steamID.IsValid())
    {
      return std::to_string(steamID.ConvertToUint64());
    }
    else if (uint64_t steamID64 = identity.GetSteamID64(); steamID64 != 0)
    {
      return std::to_string(steamID64);
    }
    throw std::runtime_error("Invalid SteamIDAddress: " +
                             std::to_string(identity.m_eType));
  }

  CSteamID get_steam_id() const
  {
    if (identity.m_eType != k_ESteamNetworkingIdentityType_SteamID)
      throw std::runtime_error("Not a SteamIDAddress");
    return identity.GetSteamID();
  }
  uint64_t get_steam_id64() const
  {
    if (identity.m_eType != k_ESteamNetworkingIdentityType_SteamID)
      throw std::runtime_error("Not a SteamIDAddress");
    return identity.GetSteamID64();
  }

  std::size_t hash() const noexcept override
  {
    return std::hash<uint64_t>{}(identity.GetSteamID64());
  }
};

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
  std::variant<std::monostate, IPv4Address, IPv6Address, SteamIDAddress>
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
  bool IsSteamID() const
  {
    return std::holds_alternative<SteamIDAddress>(address);
  }

  SteamNetworkingIPAddr ToSteamAddr() const
  {
    SteamNetworkingIPAddr steamAddr;
    bool success = steamAddr.ParseString(to_string().c_str());
      if (!success)
      {
        throw std::runtime_error("Failed to parse EndPointAddress to SteamNetworkingIPAddr");
      }
    /* if (IsIpv4())
    {
      const IPv4Address& ipv4 = std::get<IPv4Address>(address);
      uint32_t ipv4_packed = (static_cast<uint32_t>(ipv4[0]) << 24) |
                             (static_cast<uint32_t>(ipv4[1]) << 16) |
                             (static_cast<uint32_t>(ipv4[2]) << 8) |
                             static_cast<uint32_t>(ipv4[3]);
      steamAddr.SetIPv4(ipv4_packed, get_port());
    }
    else if (IsIpv6())
    {
      const IPv6Address& ipv6 = std::get<IPv6Address>(address);
      uint8_t ipv6_bytes[16];
      for (size_t i = 0; i < 16; ++i)
      {
        ipv6_bytes[i] = ipv6[i];
      }
      steamAddr.SetIPv6(ipv6_bytes, get_port());
    }
    else if (IsSteamID())
    {
      throw std::runtime_error("Invalid EndPointAddress variant");
    }
    else
    {
      throw std::runtime_error("Invalid EndPointAddress variant");
    } */
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
          std::runtime_error(
              "Parsing SteamIDAddress from string not implemented");
        }
        catch (const std::exception&)
        {
          throw std::invalid_argument("Invalid EndPointAddress: " + str);
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

// Specializations must exist before EndPointAddress::hash() is defined,
// because that definition uses std::hash<IPv4Address> etc. via std::visit.
namespace std
{
template <> struct hash<IPv4Address>
{
  std::size_t operator()(const IPv4Address& a) const noexcept
  {
    return a.hash();
  }
};
template <> struct hash<IPv6Address>
{
  std::size_t operator()(const IPv6Address& a) const noexcept
  {
    return a.hash();
  }
};
template <> struct hash<SteamIDAddress>
{
  std::size_t operator()(const SteamIDAddress& a) const noexcept
  {
    return a.hash();
  }
};
} // namespace std

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

#pragma once
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include "steam/steamtypes.h"
#include <array>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <ios>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <steam/steam_api_common.h>
#include <string>
#include <sys/types.h>
#include <variant>
namespace AtlasNet
{

namespace Detail
{
inline bool looks_like_ipv4(std::string_view s)
{
  if (s.empty())
    return false;

  bool saw_dot = false;

  for (unsigned char c : s)
  {
    if (std::isdigit(c))
      continue;

    if (c == '.')
    {
      saw_dot = true;
      continue;
    }

    return false;
  }

  return saw_dot;
}

inline bool looks_like_ipv6(std::string_view s)
{
  if (s.empty())
    return false;

  bool saw_colon = false;

  for (unsigned char c : s)
  {
    if (std::isxdigit(c))
      continue;

    if (c == ':')
    {
      saw_colon = true;
      continue;
    }

    return false;
  }

  return saw_colon;
}

inline bool looks_like_hostname(std::string_view s)
{
  if (s.empty())
    return false;

  for (unsigned char c : s)
  {
    if (std::isalnum(c) || c == '-' || c == '.')
      continue;

    return false;
  }

  return true;
}
} // namespace Detail
class IAddress
{
public:
  virtual ~IAddress() = default;
  virtual std::string to_string() const = 0;
  virtual void parse_string(const std::string& str) = 0;
  virtual std::size_t hash() const noexcept = 0;
};

class IPv4 : public IAddress
{
  std::array<uint8_t, 4> octets{};

public:
  IPv4() = default;

  explicit IPv4(uint32_t packed)
  {
    octets[0] = static_cast<uint8_t>((packed >> 24) & 0xFF);
    octets[1] = static_cast<uint8_t>((packed >> 16) & 0xFF);
    octets[2] = static_cast<uint8_t>((packed >> 8) & 0xFF);
    octets[3] = static_cast<uint8_t>(packed & 0xFF);
  }

  IPv4(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
  {
    octets[0] = o1;
    octets[1] = o2;
    octets[2] = o3;
    octets[3] = o4;
  }

  explicit IPv4(const std::string& str)
  {
    parse_string(str);
  }

  void parse_string(const std::string& str) override
  {
    if (str.empty())
      throw std::invalid_argument("IPv4 string is empty");

    std::array<uint8_t, 4> bytes{};
    size_t pos = 0;

    for (int i = 0; i < 4; ++i)
    {
      if (pos >= str.size())
        throw std::invalid_argument("IPv4 has too few octets");

      size_t start = pos;
      while (pos < str.size() && str[pos] != '.')
        ++pos;

      if (start == pos)
        throw std::invalid_argument("IPv4 has empty octet");

      unsigned value = 0;
      for (size_t j = start; j < pos; ++j)
      {
        if (!std::isdigit(static_cast<unsigned char>(str[j])))
          throw std::invalid_argument("IPv4 octet is not numeric");

        value = value * 10 + (str[j] - '0');
        if (value > 255)
          throw std::out_of_range("IPv4 octet out of range");
      }

      bytes[i] = static_cast<uint8_t>(value);

      if (i < 3)
      {
        if (pos >= str.size() || str[pos] != '.')
          throw std::invalid_argument("IPv4 has too few octets");
        ++pos;
      }
    }

    if (pos != str.size())
      throw std::invalid_argument("IPv4 has trailing characters");

    octets = bytes;
  }

  std::string to_string() const override
  {
    return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
           std::to_string(octets[2]) + "." + std::to_string(octets[3]);
  }

  bool operator==(const IPv4& other) const
  {
    return std::memcmp(octets.data(), other.octets.data(), 4) == 0;
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

class IPv6 : public IAddress
{
  std::array<uint8_t, 16> bytes{};

public:
  IPv6() = default;

  IPv6(uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5,
       uint16_t s6, uint16_t s7, uint16_t s8)
  {
    set_segment(0, s1);
    set_segment(1, s2);
    set_segment(2, s3);
    set_segment(3, s4);
    set_segment(4, s5);
    set_segment(5, s6);
    set_segment(6, s7);
    set_segment(7, s8);
  }

  IPv6(uint8_t s1_0, uint8_t s1_1, uint8_t s2_0, uint8_t s2_1, uint8_t s3_0,
       uint8_t s3_1, uint8_t s4_0, uint8_t s4_1, uint8_t s5_0, uint8_t s5_1,
       uint8_t s6_0, uint8_t s6_1, uint8_t s7_0, uint8_t s7_1, uint8_t s8_0,
       uint8_t s8_1)
  {
    bytes[0] = s1_0;
    bytes[1] = s1_1;
    bytes[2] = s2_0;
    bytes[3] = s2_1;
    bytes[4] = s3_0;
    bytes[5] = s3_1;
    bytes[6] = s4_0;
    bytes[7] = s4_1;
    bytes[8] = s5_0;
    bytes[9] = s5_1;
    bytes[10] = s6_0;
    bytes[11] = s6_1;
    bytes[12] = s7_0;
    bytes[13] = s7_1;
    bytes[14] = s8_0;
    bytes[15] = s8_1;
  }

  explicit IPv6(const uint8_t bytes_[16])
  {
    for (size_t i = 0; i < 16; ++i)
      bytes[i] = bytes_[i];
  }

  explicit IPv6(const uint16_t segments[8])
  {
    for (size_t i = 0; i < 8; ++i)
      set_segment(i, segments[i]);
  }

  explicit IPv6(const std::string& str)
  {
    parse_string(str);
  }

  void parse_string(const std::string& str) override
  {
    bytes.fill(0);

    size_t start = 0;
    for (int i = 0; i < 8; ++i)
    {
      size_t colon = str.find(':', start);

      if (colon == std::string::npos)
      {
        if (i != 7)
          throw std::invalid_argument("Invalid IPv6 address: " + str);
        colon = str.size();
      }

      const std::string segment_str = str.substr(start, colon - start);
      if (segment_str.empty())
        throw std::invalid_argument("Invalid IPv6 address: " + str);

      size_t parsed_chars = 0;
      const unsigned long value = std::stoul(segment_str, &parsed_chars, 16);

      if (parsed_chars != segment_str.size())
        throw std::invalid_argument("Invalid IPv6 segment: " + segment_str);

      if (value > 0xFFFF)
        throw std::invalid_argument("IPv6 segment out of range: " +
                                    segment_str);

      set_segment(i, static_cast<uint16_t>(value));
      start = colon + 1;
    }

    if (start != str.size() + 1)
      throw std::invalid_argument("Invalid IPv6 address: " + str);
  }

  std::string to_string() const override
  {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0');

    for (int i = 0; i < 8; ++i)
    {
      if (i > 0)
        oss << ":";
      oss << std::setw(4) << static_cast<unsigned>(segment(i));
    }

    return oss.str();
  }

  bool operator==(const IPv6& other) const
  {
    return bytes == other.bytes;
  }

  std::size_t hash() const noexcept override
  {
    std::size_t h = 0;
    for (uint8_t b : bytes)
    {
      h ^=
          std::hash<uint8_t>{}(b) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    }
    return h;
  }

  uint8_t operator[](size_t index) const
  {
    if (index >= bytes.size())
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return bytes[index];
  }

  uint8_t& operator[](size_t index)
  {
    if (index >= bytes.size())
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return bytes[index];
  }

  size_t size() const
  {
    return bytes.size();
  }

private:
  void set_segment(size_t index, uint16_t value)
  {
    if (index >= 8)
      throw std::out_of_range("IPv6 segment index out of range");

    bytes[index * 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[index * 2 + 1] = static_cast<uint8_t>(value & 0xFF);
  }

  uint16_t segment(size_t index) const
  {
    if (index >= 8)
      throw std::out_of_range("IPv6 segment index out of range");

    return static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[index * 2]) << 8) |
        static_cast<uint16_t>(bytes[index * 2 + 1]));
  }
};

class SteamIDAddress : public IAddress
{
  SteamNetworkingIdentity identity{};

public:
  SteamIDAddress() = default;

  void parse_string(const std::string& str) override
  {
    throw std::runtime_error(
        "Parsing SteamIDAddress from string not implemented: " + str);
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
      return std::to_string(steamID.ConvertToUint64());

    if (uint64_t steamID64 = identity.GetSteamID64(); steamID64 != 0)
      return std::to_string(steamID64);

    throw std::runtime_error("Invalid SteamIDAddress");
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

  bool operator==(const SteamIDAddress& other) const
  {
    return identity.GetSteamID64() == other.identity.GetSteamID64();
  }
};

class HostName : public IAddress
{
  std::string hostname;
  mutable std::optional<std::variant<IPv4, IPv6>> resolved_ip;
  mutable bool dirty = true;

public:
  HostName() = default;

  explicit HostName(std::string host) : hostname(std::move(host))
  {
    set_hostname(hostname);
  }

  void parse_string(const std::string& str) override
  {
    if (str.empty())
      throw std::invalid_argument("Hostname cannot be empty");

    if (str.size() > 253)
      throw std::invalid_argument(
          "Hostname cannot be longer than 253 characters by RFC 1034");

    hostname = str;
    dirty = true;
  }

  std::string to_string() const override
  {
    return hostname;
  }

  std::size_t hash() const noexcept override
  {
    return std::hash<std::string>{}(hostname);
  }

  const std::string& get_hostname() const noexcept
  {
    return hostname;
  }

  void set_hostname(std::string host)
  {
    if (!IsValidHostname(host))
      throw std::invalid_argument("Invalid hostname: " + host);

    hostname = std::move(host);
    dirty = true;
  }

  std::optional<std::variant<IPv4, IPv6>> resolve() const;

  bool operator==(const HostName& other) const noexcept
  {
    return hostname == other.hostname;
  }

  static bool IsValidHostname(const std::string& host)
  {
    if (host.empty() || host.size() > 253)
      return false;

    std::size_t start = 0;
    std::size_t end = host.size();

    if (host.back() == '.')
    {
      if (host.size() == 1)
        return false;
      --end;
    }

    while (start < end)
    {
      std::size_t dot = host.find('.', start);
      if (dot == std::string::npos || dot > end)
        dot = end;

      const std::size_t label_len = dot - start;
      if (label_len == 0 || label_len > 63)
        return false;

      if (host[start] == '-' || host[dot - 1] == '-')
        return false;

      for (std::size_t i = start; i < dot; ++i)
      {
        const unsigned char c = static_cast<unsigned char>(host[i]);
        if (!(std::isalnum(c) || c == '-'))
          return false;
      }

      start = dot + 1;
    }

    return true;
  }
};

class HostAddress : public IAddress
{
  std::variant<std::monostate, IPv4, IPv6, HostName, SteamIDAddress> address;

public:
  HostAddress() = default;
  explicit HostAddress(const std::string& str)
  {
    parse_string(str);
  }
  HostAddress(const IPv4& ipv4) : address(ipv4) {}
  HostAddress(const IPv6& ipv6) : address(ipv6) {}
  HostAddress(const HostName& hostName) : address(hostName) {}
  HostAddress(const SteamIDAddress& steamID) : address(steamID) {}

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

  std::string to_string() const override
  {
    return std::visit(
        [](const auto& addr) -> std::string
        {
          using T = std::decay_t<decltype(addr)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            throw std::runtime_error("HostAddress is not initialized");
          else
            return addr.to_string();
        },
        address);
  }

  void parse_string(const std::string& str) override
  {
    if (Detail::looks_like_ipv4(str))
    {
      address = IPv4(str);
      return;
    }
    if (Detail::looks_like_ipv6(str))
    {
      address = IPv6(str);
      return;
    }
    if (Detail::looks_like_hostname(str))
    {
      address = HostName(str);
      return;
    }
    throw std::invalid_argument("Invalid HostAddress: " + str);
  }

  std::size_t hash() const noexcept override
  {
    return std::visit(
        [](const auto& addr) -> std::size_t
        {
          using T = std::decay_t<decltype(addr)>;
          if constexpr (std::is_same_v<T, std::monostate>)
            return 0u;
          else
            return addr.hash();
        },
        address);
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

  bool operator==(const HostAddress& other) const
  {
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
};
} // namespace AtlasNet
namespace std
{
template <> struct hash<AtlasNet::IPv4>
{
  std::size_t operator()(const AtlasNet::IPv4& a) const noexcept
  {
    return a.hash();
  }
};

template <> struct hash<AtlasNet::IPv6>
{
  std::size_t operator()(const AtlasNet::IPv6& a) const noexcept
  {
    return a.hash();
  }
};

template <> struct hash<AtlasNet::SteamIDAddress>
{
  std::size_t operator()(const AtlasNet::SteamIDAddress& a) const noexcept
  {
    return a.hash();
  }
};

template <> struct hash<AtlasNet::HostName>
{
  std::size_t operator()(const AtlasNet::HostName& a) const noexcept
  {
    return a.hash();
  }
};

template <> struct hash<AtlasNet::HostAddress>
{
  std::size_t operator()(const AtlasNet::HostAddress& a) const noexcept
  {
    return a.hash();
  }
};
} // namespace std

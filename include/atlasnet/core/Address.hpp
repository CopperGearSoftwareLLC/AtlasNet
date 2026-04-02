#pragma once
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include "steam/steamtypes.h"
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
  bool operator==(const IPv4Address& other) const
  {
    return std::memcmp(octets, other.octets, 4) == 0;
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
  std::array<uint8_t, 16> bytes{};

public:
  IPv6Address() = default;

  IPv6Address(uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5,
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

  explicit IPv6Address(const std::string& str)
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

      std::string segment_str = str.substr(start, colon - start);
      if (segment_str.empty())
        throw std::invalid_argument("Invalid IPv6 address: " + str);

      unsigned long value = std::stoul(segment_str, nullptr, 16);
      if (value > 0xFFFF)
        throw std::invalid_argument("IPv6 segment out of range: " +
                                    segment_str);

      set_segment(i, static_cast<uint16_t>(value));
      start = colon + 1;
    }

    if (start < str.size() + 1)
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

  bool operator==(const IPv6Address& other) const
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
class DNSAddress : public IAddress
{
  std::string hostname;
  mutable std::optional<std::variant<IPv4Address, IPv6Address>>
      resolved_ip;           // optional cache of resolved IP address
  mutable bool dirty = true; // whether resolved_ip needs to be refreshed
public:
  DNSAddress() = default;

  explicit DNSAddress(std::string host) : hostname(std::move(host))
  {
    set_hostname(hostname);
  }

  void parse_string(const std::string& str) override
  {
    if (str.empty())
      throw std::invalid_argument("DNS hostname cannot be empty");

    // This class stores only the hostname, not host:port
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
      throw std::invalid_argument("Invalid DNS hostname: " + host);
    hostname = std::move(host);
    dirty = true;
  }
  std::optional<std::variant<IPv4Address, IPv6Address>> resolve() const;

  bool operator==(const DNSAddress& other) const noexcept
  {
    return hostname == other.hostname;
  }
  static bool IsValidHostname(const std::string& host)
  {
    if (host.empty() || host.size() > 253)
      return false;

    std::size_t start = 0;
    std::size_t end = host.size();

    // Allow trailing dot for FQDN, like "example.com."
    if (host.back() == '.')
    {
      if (host.size() == 1)
        return false;
      end--;
    }

    while (start < end)
    {
      std::size_t dot = host.find('.', start);
      if (dot == std::string::npos || dot > end)
        dot = end;

      std::size_t label_len = dot - start;
      if (label_len == 0 || label_len > 63)
        return false;

      if (host[start] == '-' || host[dot - 1] == '-')
        return false;

      for (std::size_t i = start; i < dot; ++i)
      {
        unsigned char c = static_cast<unsigned char>(host[i]);
        if (!(std::isalnum(c) || c == '-'))
          return false;
      }

      start = dot + 1;
    }

    return true;
  }
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
template <> struct hash<DNSAddress>
{
  std::size_t operator()(const DNSAddress& a) const noexcept
  {
    return a.hash();
  }
};
} // namespace std

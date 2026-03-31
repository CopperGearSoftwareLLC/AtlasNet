#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingtypes.h"
#include "steam/steamtypes.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <steam/steam_api_common.h>
#include <string>
#include <variant>
class IAddress {
public:
  virtual ~IAddress() = default;
  virtual std::string to_string() const = 0;
};
class IPv4Address : public IAddress {
  uint8_t octets[4];

public:
  IPv4Address(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4) {
    octets[0] = o1;
    octets[1] = o2;
    octets[2] = o3;
    octets[3] = o4;
  }
  explicit IPv4Address(const std::string &str) {
    if ("localhost" == str) {
      octets[0] = 127;
      octets[1] = 0;
      octets[2] = 0;
      octets[3] = 1;
      return;
    }
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
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

  std::string to_string() const override {
    return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
           std::to_string(octets[2]) + "." + std::to_string(octets[3]);
  }

  uint8_t operator[](size_t index) const {
    if (index >= 4)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return octets[index];
  }
  uint8_t &operator[](size_t index) {
    if (index >= 4)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return octets[index];
  }
  size_t size() const { return 4; }
};

class IPv6Address : public IAddress {

  uint16_t segments[8];

public:
  IPv6Address(uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4, uint16_t s5,
              uint16_t s6, uint16_t s7, uint16_t s8) {
    segments[0] = s1;
    segments[1] = s2;
    segments[2] = s3;
    segments[3] = s4;
    segments[4] = s5;
    segments[5] = s6;
    segments[6] = s7;
    segments[7] = s8;
  }
  explicit IPv6Address(const std::string &str) {
    size_t start = 0;
    for (int i = 0; i < 8; ++i) {
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
  std::string to_string() const override {
    std::string result;
    for (int i = 0; i < 8; ++i) {
      if (i > 0)
        result += ":";
      result += std::to_string(segments[i]);
    }
    return result;
  }
  uint8_t operator[](size_t index) const {
    if (index >= 16)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return reinterpret_cast<const uint8_t *>(segments)[index];
  }
  uint8_t &operator[](size_t index) {
    if (index >= 16)
      throw std::out_of_range("Index out of range: " + std::to_string(index));
    return reinterpret_cast<uint8_t *>(segments)[index];
  }
  size_t size() const { return 16; }
};
class SteamIDAddress : public IAddress {
  SteamNetworkingIdentity identity;

public:
  explicit SteamIDAddress(CSteamID steamID) { identity.SetSteamID(steamID); }
  explicit SteamIDAddress(uint64_t steamID) { identity.SetSteamID64(steamID); }
  std::string to_string() const override {
    if (CSteamID steamID = identity.GetSteamID(); steamID.IsValid()) {
      return std::to_string(steamID.ConvertToUint64());
    } else if (uint64_t steamID64 = identity.GetSteamID64(); steamID64 != 0) {
      return std::to_string(steamID64);
    }
    throw std::runtime_error("Invalid SteamIDAddress: " +
                             std::to_string(identity.m_eType));
  }
  CSteamID get_steam_id() const {
    if (identity.m_eType != k_ESteamNetworkingIdentityType_SteamID)
      throw std::runtime_error("Not a SteamIDAddress");
    return identity.GetSteamID();
  }
  uint64_t get_steam_id64() const {
    if (identity.m_eType != k_ESteamNetworkingIdentityType_SteamID)
      throw std::runtime_error("Not a SteamIDAddress");
    return identity.GetSteamID64();
  }
};
using PortType = uint16_t;
class IEndPointAddress {
  PortType port;

public:
  virtual ~IEndPointAddress() = default;
  PortType get_port() const { return port; }
  void set_port(PortType p) { port = p; }
  virtual std::string to_string() const = 0;
};
template <typename T>
  requires std::derived_from<T, IAddress>
class TEndPointAddress : public IEndPointAddress {
  T address;

public:
  std::string to_string() const override { return address.to_string(); }

  T get_address() const { return address; }
  void set_address(const T &addr) { address = addr; }
};
class EndPointAddress : public IEndPointAddress {
  std::variant<IPv4Address, IPv6Address, SteamIDAddress> address;

public:
EndPointAddress(const IPv4Address &ipv4, PortType port):address(ipv4) {
    set_port(port);
  }
  EndPointAddress(const IPv6Address &ipv6, PortType port):address(ipv6) {
    set_port(port);
  }
  EndPointAddress(const SteamIDAddress &steamID, PortType port):address(steamID) {
    set_port(port);
  }
  bool IsIpv4() const { return std::holds_alternative<IPv4Address>(address); }
  bool IsIpv6() const { return std::holds_alternative<IPv6Address>(address); }
  bool IsSteamID() const {
    return std::holds_alternative<SteamIDAddress>(address);
  }
  std::string to_string() const override {
    return std::visit([](const auto &addr) { return addr.to_string(); },
                      address);
  }
  template <typename T>
    requires std::derived_from<T, IAddress>
  T get_address() const {
    if (auto ptr = std::get_if<T>(&address)) {
      return ptr->get_address();
    }
    throw std::runtime_error("Address type mismatch");
  }
  template <typename T>
    requires std::derived_from<T, IAddress>
  void set_address(const T &addr) {
    address = TEndPointAddress<T>{addr};
  }
};


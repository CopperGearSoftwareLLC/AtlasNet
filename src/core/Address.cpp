#include "atlasnet/core/Address.hpp"
#include <netdb.h>
#include <variant>

std::optional<std::variant<AtlasNet::IPv4, AtlasNet::IPv6>> AtlasNet::HostName::resolve() const
{
  if (!dirty)
    return resolved_ip;

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;

  addrinfo* result = nullptr;
  const int rc = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &result);

  if (rc != 0)
  {
    resolved_ip.reset();
    dirty = false;
    return std::nullopt;
  }

  std::optional<std::variant<AtlasNet::IPv4, AtlasNet::IPv6>> resolved;

  for (addrinfo* p = result; p != nullptr; p = p->ai_next)
  {
    if (p->ai_family == AF_INET)
    {
      const auto* ipv4 = reinterpret_cast<sockaddr_in*>(p->ai_addr);
      const uint8_t* bytes =
          reinterpret_cast<const uint8_t*>(&ipv4->sin_addr.s_addr);

      resolved = AtlasNet::IPv4(bytes[0], bytes[1], bytes[2], bytes[3]);
      break;
    }
    else if (p->ai_family == AF_INET6)
    {
      const auto* ipv6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
      const uint8_t* bytes =
          reinterpret_cast<const uint8_t*>(ipv6->sin6_addr.s6_addr);

      resolved = AtlasNet::IPv6((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1],
                      (static_cast<uint16_t>(bytes[2]) << 8) | bytes[3],
                      (static_cast<uint16_t>(bytes[4]) << 8) | bytes[5],
                      (static_cast<uint16_t>(bytes[6]) << 8) | bytes[7],
                      (static_cast<uint16_t>(bytes[8]) << 8) | bytes[9],
                      (static_cast<uint16_t>(bytes[10]) << 8) | bytes[11],
                      (static_cast<uint16_t>(bytes[12]) << 8) | bytes[13],
                      (static_cast<uint16_t>(bytes[14]) << 8) | bytes[15]);
      break;
    }
  }

  ::freeaddrinfo(result);

  resolved_ip = resolved;
  dirty = false;
  return resolved_ip;
}

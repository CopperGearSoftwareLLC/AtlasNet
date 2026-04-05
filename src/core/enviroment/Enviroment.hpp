#pragma once

#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include <cstdint>
#include <cstdlib>
namespace AtlasNet
{

class EnvVars
{

public:
  const static inline uint32_t TickRate =
      std::getenv("ATLASNET_TICK_RATE")
          ? std::atoi(std::getenv("ATLASNET_TICK_RATE"))
          : 20;

  const static inline PortType RPCPort =
      std::getenv("ATLASNET_RPC_PORT")
          ? static_cast<PortType>(std::atoi(std::getenv("ATLASNET_RPC_PORT")))
          : 41001;

  const static inline HostAddress InternalDBHost =
      HostAddress(std::getenv("ATLASNET_INTERNAL_DB_HOST")
                      ? std::getenv("ATLASNET_INTERNAL_DB_HOST")
                      : "127.0.0.1");
  const static inline PortType InternalDBPort =
      std::getenv("ATLASNET_INTERNAL_DB_PORT")
          ? static_cast<PortType>(
                std::atoi(std::getenv("ATLASNET_INTERNAL_DB_PORT")))
          : 6379;
};
} // namespace AtlasNet
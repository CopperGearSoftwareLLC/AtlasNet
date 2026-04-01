#pragma once

#include "atlasnet/core/database/RedisConn.hpp"
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
  };
} // namespace AtlasNet
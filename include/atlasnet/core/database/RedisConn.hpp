#pragma once

#include "atlasnet/core/Address.hpp"
#include "sw/redis++/connection.h"
#include "sw/redis++/connection_pool.h"
#include "sw/redis++/redis.h"
#include "sw/redis++/redis_cluster.h"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <sw/redis++/redis++.h>
#include <sys/types.h>
#include <variant>
class RedisConn {
public:
  enum RedisMode { eCluster, eStandalone };
  struct Settings {
    Settings() : host(IPv4Address(127, 0, 0, 1)), Port(6379) {}
    std::variant<IPv4Address, IPv6Address> host;
    uint16_t Port;
    RedisMode Mode;
    bool ExceptionOnFailure;
    uint32_t MaxConnectRetries;

    std::string user = "default";
    std::string password;
    std::optional<std::chrono::milliseconds>
        SocketTimeout; // If not set then never timeout. block until response is
                       // received.
    std::optional<std::chrono::milliseconds>
        ConnectTimeout; // If not set then never timeout. block until connection
                        // is established.
    uint16_t PoolSize = 10;
    std::optional<std::chrono::milliseconds>
        PoolConnectionIdleTimeout; // If not set then never timeout. block until
                                   // connection is established.
  } settings;

private:
  std::variant<sw::redis::Redis, sw::redis::RedisCluster> HandleVariant;

  template <typename Func> decltype(auto) RedisFunc(Func &&func) {
    return std::visit(
        [&](auto &&handle) -> decltype(auto) {
          return std::invoke(std::forward<Func>(func),
                             std::forward<decltype(handle)>(handle));
        },
        HandleVariant);
  }

public:
  RedisConn(sw::redis::Redis redis, const Settings &settings)
      : settings(settings), HandleVariant(std::move(redis)) {}

  RedisConn(sw::redis::RedisCluster redis, const Settings &settings)
      : settings(settings), HandleVariant(std::move(redis)) {}

  static std::unique_ptr<RedisConn> Connect(const Settings &settings);

  bool Set(const std::string_view key, const std::string_view value) {
    return RedisFunc(
        [&](auto &&handle) -> bool { return handle.set(key, value); });
  }
  std::optional<std::string> Get(const std::string_view key) {
    return RedisFunc([&](auto &&handle) -> std::optional<std::string> {
      return handle.get(key);
    });
  }
};
#pragma once

#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include "sw/redis++/command_options.h"
#include "sw/redis++/connection.h"
#include "sw/redis++/connection_pool.h"
#include "sw/redis++/redis.h"
#include "sw/redis++/redis_cluster.h"
#include <chrono>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sw/redis++/redis++.h>
#include <sys/types.h>
#include <variant>
namespace AtlasNet::Database
{
  namespace Redis
  {
    class KeyValWrapper;
    class HashMapWrapper;
    class SetWrapper;
    class SortedSetWrapper;
  }

class RedisConn
{
  friend class Redis::KeyValWrapper;
  friend class Redis::HashMapWrapper;
  friend class Redis::SetWrapper;
  friend class Redis::SortedSetWrapper;
public:
  enum RedisMode
  {
    eCluster,
    eStandalone
  };
  struct Settings
  {

    HostAddress host;
    PortType port;
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

  template <typename Func> decltype(auto) RedisFunc(Func&& func)
  {
    return std::visit(
        [&](auto&& handle) -> decltype(auto)
        {
          return std::invoke(std::forward<Func>(func),
                             std::forward<decltype(handle)>(handle));
        },
        HandleVariant);
  }

public:
  RedisConn(sw::redis::Redis redis, const Settings& settings);

  RedisConn(sw::redis::RedisCluster redis, const Settings& settings);
  ~RedisConn();

  static std::unique_ptr<RedisConn> Connect(const Settings& settings);

  Redis::KeyValWrapper& KeyVal();
  Redis::HashMapWrapper& HashMap();
  Redis::SetWrapper& Set();
  Redis::SortedSetWrapper& SortedSet();

private:
std::unique_ptr<Redis::KeyValWrapper> keyValWrapper;
std::unique_ptr<Redis::HashMapWrapper> hashMapWrapper;
std::unique_ptr<Redis::SetWrapper> setWrapper;
std::unique_ptr<Redis::SortedSetWrapper> sortedSetWrapper;
};

} // namespace AtlasNet::Database
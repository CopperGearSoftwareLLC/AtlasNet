#pragma once
#include "RedisConnection.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "Misc/Singleton.hpp"

class Redis : public Singleton<Redis>
{
   struct Options
   {
      std::string host;
      int32_t port;


        bool operator<(const Options& o) const
        {
         return host < o.host || port < o.port;
        }
        uint32_t ConnectRetries = 1;
        uint32_t RetryInternalMs;
   
   };
   
   std::map<Options, std::weak_ptr<RedisConnection>> redis_connections;
public:
Redis() = default;
   std::shared_ptr<RedisConnection> Connect(const Options& options);
   std::shared_ptr<RedisConnection> Connect(const std::string& address, int32_t port);
};
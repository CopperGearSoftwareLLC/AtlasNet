#include "atlasnet/core/database/redis/RedisConn.hpp"

#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/database/redis/HashMapWrapper.hpp"
#include "atlasnet/core/database/redis/KeyValWrapper.hpp"
#include "atlasnet/core/database/redis/SetWrapper.hpp"
#include "atlasnet/core/database/redis/SortedSetWrapper.hpp"
std::unique_ptr<AtlasNet::Database::RedisConn>
AtlasNet::Database::RedisConn::Connect(const Settings& settings)
{
  sw::redis::ConnectionOptions opts;
  opts.host = settings.host.to_string();
  opts.port = settings.port;
  if (settings.SocketTimeout)
  {
    opts.socket_timeout = *settings.SocketTimeout;
  }
  if (settings.ConnectTimeout)
  {
    opts.connect_timeout = *settings.ConnectTimeout;
  }

  opts.user = settings.user;
  opts.password = settings.password;
  sw::redis::ConnectionPoolOptions pool_opts;
  pool_opts.size = settings.PoolSize;
  if (settings.PoolConnectionIdleTimeout)
  {
    pool_opts.connection_idle_time = *settings.PoolConnectionIdleTimeout;
  }

  if (settings.Mode == RedisMode::eCluster)
  {

    auto cluster = sw::redis::RedisCluster(opts, pool_opts);
    return std::make_unique<RedisConn>(std::move(cluster), settings);
  }
  else
  {
    auto redis = sw::redis::Redis(opts, pool_opts);
    return std::make_unique<RedisConn>(std::move(redis), settings);
  }
}
AtlasNet::Database::Redis::KeyValWrapper&
AtlasNet::Database::RedisConn::KeyVal()
{
  AN_ASSERT(keyValWrapper, "KeyValWrapper is not initialized");
  return *keyValWrapper;
}
AtlasNet::Database::Redis::HashMapWrapper&
AtlasNet::Database::RedisConn::HashMap()
{
  AN_ASSERT(hashMapWrapper, "HashMapWrapper is not initialized");
  return *hashMapWrapper;
}
AtlasNet::Database::Redis::SetWrapper& AtlasNet::Database::RedisConn::Set()
{
  AN_ASSERT(setWrapper, "SetWrapper is not initialized");
  return *setWrapper;
}
AtlasNet::Database::Redis::SortedSetWrapper&
AtlasNet::Database::RedisConn::SortedSet()
{
  AN_ASSERT(sortedSetWrapper, "SortedSetWrapper is not initialized");
  return *sortedSetWrapper;
}
AtlasNet::Database::RedisConn::~RedisConn() {}
AtlasNet::Database::RedisConn::RedisConn(sw::redis::Redis redis,
                                         const Settings& settings)
    : settings(settings), HandleVariant(std::move(redis))
{
  keyValWrapper =
      std::unique_ptr<Redis::KeyValWrapper>(new Redis::KeyValWrapper(*this));
  hashMapWrapper =
      std::unique_ptr<Redis::HashMapWrapper>(new Redis::HashMapWrapper(*this));
  setWrapper = std::unique_ptr<Redis::SetWrapper>(new Redis::SetWrapper(*this));
  sortedSetWrapper = std::unique_ptr<Redis::SortedSetWrapper>(
      new Redis::SortedSetWrapper(*this));
}
AtlasNet::Database::RedisConn::RedisConn(sw::redis::RedisCluster redis,
                                         const Settings& settings)
    : settings(settings), HandleVariant(std::move(redis))
{
  keyValWrapper =
      std::unique_ptr<Redis::KeyValWrapper>(new Redis::KeyValWrapper(*this));
  hashMapWrapper =
      std::unique_ptr<Redis::HashMapWrapper>(new Redis::HashMapWrapper(*this));
  setWrapper = std::unique_ptr<Redis::SetWrapper>(new Redis::SetWrapper(*this));
  sortedSetWrapper = std::unique_ptr<Redis::SortedSetWrapper>(
      new Redis::SortedSetWrapper(*this));
}

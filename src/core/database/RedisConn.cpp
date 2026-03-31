#include "atlasnet/core/database/RedisConn.hpp"


 std::unique_ptr<RedisConn> RedisConn::Connect(const Settings &settings) {
    sw::redis::ConnectionOptions opts;
    opts.host = std::visit([](const auto &addr) { return addr.to_string(); },
                           settings.host);
    opts.port = settings.Port;
    if (settings.SocketTimeout) {
      opts.socket_timeout = *settings.SocketTimeout;
    }
    if (settings.ConnectTimeout) {
      opts.connect_timeout = *settings.ConnectTimeout;
    }

    opts.user = settings.user;
    opts.password = settings.password;
    sw::redis::ConnectionPoolOptions pool_opts;
    pool_opts.size = settings.PoolSize;
    if (settings.PoolConnectionIdleTimeout) {
      pool_opts.connection_idle_time = *settings.PoolConnectionIdleTimeout;
    }

    if (settings.Mode == RedisMode::eCluster) {

      auto cluster = sw::redis::RedisCluster(opts, pool_opts);
      return std::make_unique<RedisConn>(std::move(cluster), settings);
    } else {
      auto redis = sw::redis::Redis(opts, pool_opts);
      return std::make_unique<RedisConn>(std::move(redis), settings);
    }
  }
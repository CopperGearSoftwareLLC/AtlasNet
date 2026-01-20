#include "Redis.hpp"

#include <sw/redis++/async_redis.h>
#include <sw/redis++/async_redis_cluster.h>
#include <sw/redis++/connection.h>
#include <sw/redis++/redis.h>
#include <sw/redis++/redis_cluster.h>

#include <iostream>
#include <memory>
static bool LooksLikeNotClusterError(const std::string& msg)
{
	std::string s;
	s.reserve(msg.size());
	for (char c : msg) s.push_back((char)std::tolower((unsigned char)c));

	// Common cases:
	// - "ERR This instance has cluster support disabled"
	// - "unknown command `cluster`"
	return (s.find("cluster support disabled") != std::string::npos) ||
		   (s.find("unknown command") != std::string::npos);
} /*
	 try
	 {
		 std::unique_ptr<sw::redis::RedisCluster> Handle =
			 std::make_unique<sw::redis::RedisCluster>(options, pool);
		 std::unique_ptr<sw::redis::AsyncRedisCluster> HandleAsync =
			 std::make_unique<sw::redis::AsyncRedisCluster>(options, pool);

		 // If CLUSTER INFO works, it's a cluster-enabled node.
		 // redis++ supports command<T>(...) via the generic command interface.
		 // :contentReference[oaicite:1]{index=1}
		 (void)Handle->command<std::string>("cluster",
											"info");	 // :contentReference[oaicite:2]{index=2}
		 std::shared_ptr<RedisConnection> redisC =
			 std::make_shared<RedisConnection>(std::move(Handle), std::move(HandleAsync));
		 redis_connections.emplace(in_options, redisC);
		 return redisC;
	 }
	 catch (const sw::redis::ReplyError& e)
	 {
		 std::cerr << std::format("FAiled to connect to redis as cluster at {}{}", options.host,
								  options.port)
				   << std::endl;
		 // Standalone Redis (or cluster disabled) will typically error on CLUSTER INFO.
		 if (!LooksLikeNotClusterError(e.what()))
		 {
			 // Something else went wrong (auth, network, etc.) → surface it.
			 throw;
		 }*/
std::shared_ptr<RedisConnection> Redis::Connect(const Options& in_options, uint32_t max_retries,
												uint32_t retry_interval_ms)
{
	// ---- Fast path (locked, no I/O) ----
	std::lock_guard<std::mutex> lock(connections_mutex);
	{
		auto it = redis_connections.find(in_options);
		if (it != redis_connections.end())
		{
			if (auto cached = it->second.lock())
				return cached;
		}
	}

	// ---- Slow path: create connection without holding mutex ----
	sw::redis::ConnectionOptions options;
	options.host = in_options.host;
	options.port = in_options.port;

	sw::redis::ConnectionPoolOptions pool;
	pool.size = 5;

	uint32_t attempt = 0;

	for (;;)
	{
		try
		{
			std::shared_ptr<RedisConnection> redisC;

			if (in_options.IsCluster)
			{
				auto handle = std::make_unique<sw::redis::RedisCluster>(options, pool);
				auto handleAsync = std::make_unique<sw::redis::AsyncRedisCluster>(options, pool);

				handle->for_each([](sw::redis::Redis& r) { r.ping(); });

				redisC =
					std::make_shared<RedisConnection>(std::move(handle), std::move(handleAsync));
			}
			else
			{
				auto handle = std::make_unique<sw::redis::Redis>(options, pool);
				auto handleAsync = std::make_unique<sw::redis::AsyncRedis>(options, pool);

				handle->ping();

				redisC =
					std::make_shared<RedisConnection>(std::move(handle), std::move(handleAsync));
			}
			redisC->Set("test", "test");
			redisC->Del("test");
			auto& slot = redis_connections[in_options];

			// Another thread may have won the race
			if (auto existing = slot.lock())
				return existing;

			slot = redisC;

			std::cerr << "Successfully Connected to Redis\n";
			return redisC;
		}
		catch (const sw::redis::Error& e)
		{
			if (attempt >= max_retries)
				throw;

			++attempt;

			std::cerr << std::format("Redis connect failed (attempt {}/{}): {}", attempt,
									 max_retries + 1, e.what())
					  << std::endl;

			if (retry_interval_ms > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
			}
		}
	}
}

std::shared_ptr<RedisConnection> Redis::ConnectNonCluster(const std::string& address, int32_t port,
														  uint32_t max_retries,
														  uint32_t retry_interval_ms)
{
	Options op;
	op.host = address;
	op.port = port;
	op.IsCluster = false;
	return Connect(op, max_retries, retry_interval_ms);
}

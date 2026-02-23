#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Global/Misc/Singleton.hpp"
#include "RedisConnection.hpp"

class Redis : public Singleton<Redis>
{
	struct Options
	{
		std::string host;
		int32_t port;
		bool IsCluster = false;

		bool operator<(const Options& o) const
		{
			if (host != o.host)
				return host < o.host;
			if (port != o.port)
				return port < o.port;
			return IsCluster < o.IsCluster;
		}

		uint32_t ConnectRetries = 1;
		uint32_t RetryInternalMs;
	};

	std::map<Options, std::weak_ptr<RedisConnection>> redis_connections;
	std::mutex connections_mutex;  // 🔒 NEW

   public:
	std::shared_ptr<RedisConnection> Connect(const Options& options, uint32_t max_retries = 0,
											 uint32_t retry_interval_ms = 0);
	std::shared_ptr<RedisConnection> ConnectNonCluster(const std::string& address, int32_t port,
													   uint32_t max_retries = 0,
													   uint32_t retry_interval_ms = 0);
};

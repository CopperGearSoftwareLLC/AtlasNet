#include "InternalDB.hpp"

#include <cstdlib>
#include <string>

#include "Database/Redis/Redis.hpp"

namespace
{
uint32_t ResolveRetryCount()
{
	if (const char* retryText = std::getenv("INTERNAL_REDIS_CONNECT_MAX_RETRIES");
		retryText && *retryText)
	{
		char* end = nullptr;
		const long parsed = std::strtol(retryText, &end, 10);
		if (end && *end == '\0' && parsed >= 0)
		{
			return static_cast<uint32_t>(parsed);
		}
	}
	return 180;
}

uint32_t ResolveRetryIntervalMs()
{
	if (const char* retryText = std::getenv("INTERNAL_REDIS_CONNECT_RETRY_INTERVAL_MS");
		retryText && *retryText)
	{
		char* end = nullptr;
		const long parsed = std::strtol(retryText, &end, 10);
		if (end && *end == '\0' && parsed >= 0)
		{
			return static_cast<uint32_t>(parsed);
		}
	}
	return 1000;
}

std::string ResolveRedisHost()
{
	if (const char* host = std::getenv("INTERNAL_REDIS_SERVICE_NAME");
		host && *host)
	{
		return std::string(host);
	}
	return _INTERNAL_REDIS_SERVICE_NAME;
}

int32_t ResolveRedisPort()
{
	if (const char* portText = std::getenv("INTERNAL_REDIS_PORT");
		portText && *portText)
	{
		char* end = nullptr;
		const long parsed = std::strtol(portText, &end, 10);
		if (end && *end == '\0' && parsed > 0 && parsed <= 65535)
		{
			return static_cast<int32_t>(parsed);
		}
	}
	return _INTERNAL_REDIS_PORT;
}
}  // namespace

InternalDB::InternalDB()
{
	redis = Redis::Get().ConnectNonCluster(ResolveRedisHost(), ResolveRedisPort(),
											ResolveRetryCount(), ResolveRetryIntervalMs());
};

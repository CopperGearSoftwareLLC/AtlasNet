#include "InternalDB.hpp"

#include <cstdlib>
#include <string>

#include "Database/Redis/Redis.hpp"

namespace
{
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
											20, 500);
};

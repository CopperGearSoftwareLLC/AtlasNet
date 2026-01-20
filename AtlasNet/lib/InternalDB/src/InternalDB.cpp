#include "InternalDB.hpp"

#include "Redis/Redis.hpp"
InternalDB::InternalDB()
{
	redis = Redis::Get().ConnectNonCluster(_INTERNAL_REDIS_SERVICE_NAME, _INTERNAL_REDIS_PORT, 20,500);
};

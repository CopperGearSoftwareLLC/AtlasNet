#include "BuiltInDB.hpp"

#include "Redis/Redis.hpp"
BuiltInDB::BuiltInDB()
{
	transient = Redis::Get().ConnectNonCluster(_BUILTINDB_REDIS_SERVICE_NAME, _BUILTINDB_REDIS_PORT,10,200);
}

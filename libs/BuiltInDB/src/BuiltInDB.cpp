#include "BuiltInDB.hpp"

#include "Redis/Redis.hpp"
BuiltInDB::BuiltInDB()
{
	transient = Redis::Get().Connect(_BUILTINDB_REDIS_SERVICE_NAME, _BUILTINDB_REDIS_PORT);
}

#pragma once
#include "Database/Redis/RedisConnection.hpp"
#include "Global/Misc/Singleton.hpp"
class InternalDB :public Singleton<InternalDB>
{
    std::shared_ptr<RedisConnection> redis;
    public:
	 InternalDB();

     std::shared_ptr<RedisConnection> operator->() {return redis;}
};
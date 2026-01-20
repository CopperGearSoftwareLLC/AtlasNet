#pragma once
#include "Redis/RedisConnection.hpp"
#include "Misc/Singleton.hpp"
class InternalDB :public Singleton<InternalDB>
{
    std::shared_ptr<RedisConnection> redis;
    public:
	 InternalDB();

     std::shared_ptr<RedisConnection> operator->() {return redis;}
};
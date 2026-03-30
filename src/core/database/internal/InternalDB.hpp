#pragma once

#include "atlasnet/core/Singleton.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include <memory>
class InternalDB : public Singleton<InternalDB>
{
    std::unique_ptr<RedisConn> Redis;
public:
    InternalDB()
    {
        RedisConn::Settings settings;
        settings.Mode = RedisConn::RedisMode::eStandalone;
        settings.ExceptionOnFailure = true;
        settings.MaxConnectRetries = 5;
        settings.host = IPv4Address(127, 0, 0, 1);
        settings.Port = 6379;

        Redis = RedisConn::Connect(settings);
    }
};
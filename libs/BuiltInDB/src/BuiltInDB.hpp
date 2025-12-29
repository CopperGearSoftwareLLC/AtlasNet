#pragma once
#include <memory>
#include "Misc/Singleton.hpp"
#include "RedisConnection.hpp"

class BuiltInDB : public Singleton<BuiltInDB>
{
    std::shared_ptr<RedisConnection> transient;

    public:
    BuiltInDB();


    std::shared_ptr<RedisConnection> Transient() const {return transient;}
};
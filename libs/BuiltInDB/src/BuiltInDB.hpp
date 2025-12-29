#pragma once
#include <memory>
#include "Misc/Singleton.hpp"
#include "Redis/RedisConnection.hpp"

class BuiltInDB : public Singleton<BuiltInDB>
{
    std::shared_ptr<RedisConnection> transient;

    public:
	 BuiltInDB();

	 [[nodiscard]] std::shared_ptr<RedisConnection> Transient() const { return transient; }
	 [[nodiscard]] bool Persistent() const { return true; }
};
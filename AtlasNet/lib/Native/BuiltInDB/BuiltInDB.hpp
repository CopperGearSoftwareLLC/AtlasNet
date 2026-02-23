#pragma once
#include <memory>
#include "Global/Misc/Singleton.hpp"
#include "Database/Redis/RedisConnection.hpp"

class BuiltInDB : public Singleton<BuiltInDB>
{
    std::shared_ptr<RedisConnection> transient;

    public:
	 BuiltInDB();

	 [[nodiscard]] std::shared_ptr<RedisConnection> Transient() const { return transient; }
	 [[nodiscard]] bool Persistent() const { return true; }
};
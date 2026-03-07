#pragma once

#include <cstdint>

#include "Debug/Log.hpp"

#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
class ShardService : public Singleton<ShardService>
{
	Log logger = Log("ShardService");
	uint32_t activeShardCount = 0;

   private:
	void Internal_ScaleShardServiceKubernetes(uint32_t newShardCount);
	void Internal_ScaleShardServiceDockerSwarm(uint32_t newShardCount);

   public:
	void ScaleShardService(uint32_t newShardCount);
};
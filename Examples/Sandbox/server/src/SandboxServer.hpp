#pragma once

#include <SandboxWorld.hpp>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
class SandboxServer
{
	Log logger = Log("Sandbox");
	SandboxWorld world;
	bool ShouldShutdown = false;

   public:
	void Run();

	
};
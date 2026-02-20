#pragma once

#include <SandboxWorld.hpp>
#include "Debug/Log.hpp"
class SandboxServer
{
    Log logger = Log("Sandbox");
    SandboxWorld world;
    bool ShouldShutdown = false;
    public:
    void Run();
};
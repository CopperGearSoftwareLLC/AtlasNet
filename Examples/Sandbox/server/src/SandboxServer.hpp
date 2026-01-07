#pragma once

#include <SandboxWorld.hpp>
class SandboxServer
{
    SandboxWorld world;
    bool ShouldShutdown = false;
    public:
    void Run();
};
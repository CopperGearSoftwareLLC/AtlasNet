#pragma once
#include "pch.hpp"
#include <SandboxWorld.hpp>
class SandboxServer
{
    SandboxWorld world;
    bool ShouldShutdown = false;
    public:
    void Run();
};
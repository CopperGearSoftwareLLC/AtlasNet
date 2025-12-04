#pragma once
#include "pch.hpp"
#include <Sandbox/SandboxWorld.hpp>
#include "Sandbox/SandboxWorld.hpp"
class SandboxServer
{
    SandboxWorld world;
    bool ShouldShutdown = false;
    public:
    void Run();
};
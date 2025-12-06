#pragma once
#include "pch.hpp"
#include "AtlasNetClient.hpp"

class SandboxClient
{
    mutable Log logger = Log("SandboxClient");
    std::optional<AtlasEntityID> MyID;

    std::optional<GLFWwindow *> window;
    vec2 CameraPos = {0,0};
    float CameraSizeX = 10;
    bool ShouldDisconnect = false;
    bool GUIEnabled = false;
    void Startup();
    void SetImGui();
    void RenderView();
public:
    struct RunArgs
    {
        IPAddress GameCoordinatorIP;
    };
    RunArgs _run_args;
    void Run(const RunArgs &args);
    Log &GetLogger() const { return logger; }
};

#pragma once
#include "Entity.hpp"
#include "pch.hpp"
#include "AtlasNetClient.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
class SandboxClient
{
    mutable Log logger = Log("SandboxClient");
    std::optional<AtlasEntity::EntityID> MyID;

    std::optional<GLFWwindow *> window;
    vec2 CameraPos = {0,0};
    float CameraSizeX = 10;
    bool ShouldDisconnect = false;
    bool GUIEnabled = false;
    void Startup();
    void SetImGui();
    void RenderView();
public:

    void Run();
    Log &GetLogger() const { return logger; }
};

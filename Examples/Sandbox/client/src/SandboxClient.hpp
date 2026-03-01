#pragma once
#include "Command/NetCommand.hpp"
#include "Commands/GameStateCommand.hpp"
#include "Entity/Entity.hpp"
#include "Global/pch.hpp"
#include "AtlasNetClient.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
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
	void OnGameStateCommand(const NetServerStateHeader& header, const GameStateCommand& command);

   public:

    void Run(const IPAddress& address);
    Log &GetLogger() const { return logger; }
};

#pragma once
#include <string>
#include "lib_include.h"
class App
{
public:
    struct AppProperties
    {
        std::string AppName = "Default Name";
        uint32_t winSizeX =1280,winSizeY = 720;
        bool compatability_openGL_profile = true;
        bool imgui_docking_enable = false;
        bool imgui_viewports_enable = false;
        bool window_resizable = true;
        uint32_t GL_version_major = 3,GL_version_minor = 2; //this would be 3.1
    };

protected:
    AppProperties properties;
    bool shouldShutdown = false;
    GLFWwindow* window = nullptr;
    ImGuiContext* imgui_context = nullptr;
    ImGuiIO* imgui_io = nullptr;
    ImPlotContext* implot_context = nullptr;
public:
    App(const AppProperties &_p);
    void Run();
    /// @brief Commands the app to shutdown when the frame is done rendering
    void Shutdown();
ivec2 getWindowSize();
private:
    virtual void OnStart() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnPostRender() {}
    virtual void OnShutdown()= 0;
    void CleanUp();
    static void glfw_error_callback(int error, const char *description);
    
};
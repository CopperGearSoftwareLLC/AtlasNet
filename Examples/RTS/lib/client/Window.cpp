#include "Window.hpp"
#include "GL/VertexArray.hpp"
#include "GLFW/glfw3.h"
Window::Window()
{
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    const auto glfw_result = glfwInit();
    ASSERT(glfw_result == GLFW_TRUE, "GL FAILURE");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1280, 720, "RTS", nullptr, nullptr);
    ASSERT(window, "Window creation failure");

    glfwMakeContextCurrent(window);
    glewInit();

    // Set the resize callback
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Set initial viewport
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    // ImGui setup
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImFontConfig fontconfig;
    fontconfig.SizePixels = 20.0f;
    io.Fonts->AddFontDefault(&fontconfig);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    glfwSwapInterval(1);
}

void Window::Resize(int width, int height)
{
    glViewport(0, 0, width, height);
    glfwSetWindowSize(window, width, height);
}

// GLFW callback
void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
void Window::PreRender()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
}
void Window::PostRender()
{
	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(window);
	glfwPollEvents();
}

#include "App.h"

#include <GLFW/glfw3.h>
#include <steam/steamtypes.h>

#include <exception>
#include <iostream>
void APIENTRY openglDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
								  GLsizei length, const GLchar *message, const void *userParam)
{
	std::cerr << "OpenGL Debug Message [" << id << "]: " << message << std::endl;

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH:
			std::cerr << "Severity: HIGH" << std::endl;
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			std::cerr << "Severity: MEDIUM" << std::endl;
			break;
		case GL_DEBUG_SEVERITY_LOW:
			std::cerr << "Severity: LOW" << std::endl;
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			std::cerr << "Severity: NOTIFICATION" << std::endl;
			break;
	}

	std::cerr << "------------------------" << std::endl;
}
App::App(const AppProperties &_p) : properties(_p)
{
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
	if (glfwInit() != GLFW_TRUE)
	{
		throw std::runtime_error("GLEW failed to init");
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, properties.GL_version_major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, properties.GL_version_minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, properties.compatability_openGL_profile
											? GLFW_OPENGL_COMPAT_PROFILE
											: GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // required for OSX
	glfwWindowHint(GLFW_RESIZABLE, properties.window_resizable);
	glfwSetErrorCallback(glfw_error_callback);
	ivec2 WorkAreaPos;
	ivec2 WorkAreaSize;
	glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &WorkAreaPos.x, &WorkAreaPos.y, &WorkAreaSize.x,
						   &WorkAreaSize.y);
     properties.winSizeX = glm::min((int32)properties.winSizeX,WorkAreaSize.x);                      
     properties.winSizeY = glm::min((int32)properties.winSizeY,WorkAreaSize.y);                      
	window = glfwCreateWindow(properties.winSizeX, properties.winSizeY, properties.AppName.c_str(),
							  NULL, NULL);
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK)
	{
		throw std::runtime_error("GLEW failed to init");
	}
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);	// Ensures messages are synchronous
	glDebugMessageCallback(openglDebugCallback, nullptr);
	const GLubyte *version = glGetString(GL_VERSION);
	std::cout << "OpenGL version supported: " << version << std::endl;

	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 150");
	ImGui::StyleColorsDark();
	ImPlot::CreateContext();
	auto &io = ImGui::GetIO();
	vec2 content_scale;
	glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &content_scale.x, &content_scale.y);
	ImGui::GetStyle().ScaleAllSizes(content_scale.x);
	ImGui::GetIO().FontGlobalScale = content_scale.x;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	io.ConfigFlags |=
		ImGuiConfigFlags_DockingEnable * properties.imgui_docking_enable;  // Enable Docking
	io.ConfigFlags |=
		ImGuiConfigFlags_ViewportsEnable *
		properties.imgui_viewports_enable;	// Enable Multi-Viewport / Platform Windows
}

void App::Run()
{
	auto &io = ImGui::GetIO();

	OnStart();
	while (!shouldShutdown && !glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		// new imgui frame

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		OnUpdate();
		OnRender();

		ImGui::EndFrame();
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow *backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
		OnPostRender();
		glfwSwapBuffers(window);
	}
	OnShutdown();
	CleanUp();
}

void App::Shutdown()
{
	shouldShutdown = true;
}

void App::CleanUp()
{
	ImPlot::DestroyContext();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
}

void App::glfw_error_callback(int error, const char *description)
{
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

ivec2 App::getWindowSize()
{
	ivec2 size;
	glfwGetFramebufferSize(window, &size.x, &size.y);
	return size;
}

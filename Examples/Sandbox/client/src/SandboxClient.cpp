#include "SandboxClient.hpp"
#include "Network/IPAddress.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <thread>
void SandboxClient::Startup()
{
	SetImGui();
	if (!GUIEnabled)
	{
		logger.Warning("failed to initialize GUI. Running headless");
	}
	AtlasNetClient::InitializeProperties properties;
	properties.AtlasNetProxyIP = IPAddress::MakeLocalHost(_PORT_PROXY_PUBLISHED);
	AtlasNetClient::Get().Initialize(properties);
}

void SandboxClient::SetImGui()
{
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
	const auto glfw_result = glfwInit();
	if (glfw_result != GLFW_TRUE)
	{
		logger.Warning("failed to init glfw");
		GUIEnabled = false;
		return;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(1280, 720, "Cartograph", nullptr, nullptr);
	if (!window)
	{
		logger.Warning("failed to create window");
		GUIEnabled = false;
		return;
	}
	glfwMakeContextCurrent(*window);
	glewInit();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	ImFontConfig fontconfig;
	fontconfig.SizePixels = 20.0f;
	io.Fonts->AddFontDefault(&fontconfig);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForOpenGL(*window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
	GUIEnabled = true;
}

void SandboxClient::RenderView()
{
	ImGuiWindowFlags window_flags = 
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	//ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::Begin("MainWindow", nullptr, window_flags);
	ImGui::PopStyleVar(2);
	const auto avail_space = ImGui::GetContentRegionAvail();
	ImGui::SetNextWindowSizeConstraints(ImVec2(avail_space.x * 0.4f, 0),
										ImVec2(avail_space.x * 0.9f, avail_space.y));
	ImGui::BeginChild("game view", {0, 0});
	if (ImPlot::BeginPlot("view", ImVec2(-1, -1),
						  ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoTitle |
							  ImPlotFlags_NoInputs | ImPlotFlags_Equal))
	{
		static auto plot_size = ImVec2(1, 1);
		const float Aspect_Ratio = plot_size.y / (float)plot_size.x;
		ImPlot::SetupAxesLimits(CameraPos.x - CameraSizeX, CameraPos.x + CameraSizeX,
								CameraPos.y - CameraSizeX * Aspect_Ratio,
								CameraPos.y + CameraSizeX * Aspect_Ratio, ImGuiCond_Always);
		plot_size = ImPlot::GetPlotSize();
		const float s_v =
			((ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f) +
			 (ImGui::IsKeyDown(ImGuiKey_E) ? -1.0f : 0.0f) - ImGui::GetIO().MouseWheel);
		CameraSizeX = glm::max(1.0f, CameraSizeX + s_v);
		const bool wPressed = ImGui::IsKeyDown(ImGuiKey_W), sPressed = ImGui::IsKeyDown(ImGuiKey_S),
				   aPressed = ImGui::IsKeyDown(ImGuiKey_A), dPressed = ImGui::IsKeyDown(ImGuiKey_D);
		const float x_v = dPressed + (aPressed ? -1.0f : 0.0f);
		const float y_v = wPressed + (sPressed ? -1.0f : 0.0f);
		CameraPos += vec2(x_v, y_v) * 0.02f * CameraSizeX;
		ImPlot::GetPlotDrawList()->AddCircleFilled(
			ImPlot::PlotToPixels(ImVec2(CameraPos.x, CameraPos.y)), 100.0f * 1 / CameraSizeX,
			ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 1, 1)));
		ImPlot::EndPlot();
	}
	ImGui::EndChild();
	ImGui::End();
}

void SandboxClient::Run()
{

	Startup();

	using clock = std::chrono::high_resolution_clock;
	auto previous = clock::now();
	if (GUIEnabled)
	{
		ShouldDisconnect = ShouldDisconnect || glfwWindowShouldClose(*window);
	}
	while (!ShouldDisconnect)
	{
		auto now = clock::now();
		std::chrono::duration<float> delta = now - previous;
		previous = now;
		float dt = delta.count();  // seconds

		AtlasNetClient::Get().Tick();

		// Print positions every second
		// scene.printPositions();

		// Sleep a bit to avoid burning CPU (simulate frame time)
		// std::this_thread::sleep_for(std::chrono::milliseconds(16));
		// ~60 updates per second
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		if (GUIEnabled)
		{
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			//ImGui::ShowDemoWindow();
			RenderView();
			ImGui::EndFrame();
			ImGui::Render();

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(*window);
			glfwPollEvents();
		}
	}
}

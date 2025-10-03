#include <GodView/GodView.hpp>
#include <GodView/PartitionVisualization.hpp>
#include <Debug/Crash/CrashHandler.hpp>
int main(int argc, char** argv)
{
    CrashHandler::Get().Init(argv[0]);

    std::cerr << "Hello from AtlasView\n";
    
    // Initialize partition visualization
    PartitionVisualization partitionViz;
    
  
    //glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(1200, 800, "GodView", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    std::cerr << "Vendor: " << glGetString(GL_VENDOR);
    glewInit();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    while (!glfwWindowShouldClose(window)) {
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);

	ImGui::Begin("GodView", nullptr,
		     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
	{
	    // Update partition visualization
	    static float deltaTime = 0.016f; // Approximate 60 FPS
	    partitionViz.update(deltaTime);
	    
	    // Render partition visualization
	    partitionViz.render();
	}
	ImGui::End();
	
	// Optional: Show demo windows for debugging
	// ImGui::ShowDemoWindow();
	// ImPlot::ShowDemoWindow();
	
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwPollEvents();
	glfwSwapBuffers(window);
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	    GLFWwindow *backup_current_context = glfwGetCurrentContext();
	    ImGui::UpdatePlatformWindows();
	    ImGui::RenderPlatformWindowsDefault();
	    glfwMakeContextCurrent(backup_current_context);
	}
    }
 
}
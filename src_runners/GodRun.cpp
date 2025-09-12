#include "God/God.hpp"
#include "Partition/Partition.hpp"
#include "pch.hpp"

int main(int argc, char **argv) {
    std::cerr << "Hello\n";
    std::cerr << "From\n";
    std::cerr << "God\n";

    for (int i = 0; i < argc; i++) {
	std::cerr << argv[i] << std::endl;
    }

    God god;

    // Example: spawn 4 partitions
    for (int i = 1; i <= 4; i++) {
        int port = 7000 + i;
        god.spawnPartition(i, port);
    }

    std::this_thread::sleep_for(std::chrono::seconds(100));  
    god.cleanupPartitions();
    return 0;
}

/*
int main(int argc, char **argv) {
    std::cerr << "Hello\n";
    std::cerr << "From\n";
    std::cerr << "God\n";

    for (int i = 0; i < argc; i++) {
	std::cerr << argv[i] << std::endl;
    }

#ifdef _DISPLAY
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 800, "GodView", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    std::cerr << "Vendor: " << glGetString(GL_VENDOR);
    glewInit();
    ImGui::CreateContext();
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
	    const auto availableSpace = ImGui::GetContentRegionAvail();
	    ImGui::BeginChild("MapView", ImVec2(availableSpace.x * (2.0f / 3.0f), 0),
			      ImGuiChildFlags_ResizeX);
	    ImGui::Button("hello");
	    ImGui::EndChild();
	    ImGui::SameLine();
	    ImGui::BeginChild("PartitionGrid");
	    ImGui::TextUnformatted("Not a button");
	    ImGui::EndChild();
	}
	ImGui::End();
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
#else
#endif
}
*/
// RedClearImGui.cpp
#include <emscripten/html5.h>
#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#include <emscripten/emscripten.h>
#else
#error "This file is intended to be built with Emscripten for the web."
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static GLFWwindow* g_window = nullptr;
static float g_clear_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
static bool g_show_demo = false;

struct CanvasSizes {
  int fb_w = 0, fb_h = 0;      // drawing buffer (device pixels)
  float css_w = 0, css_h = 0;  // CSS pixels
  float scale_x = 1, scale_y = 1; // fb/css
};

static CanvasSizes ResizeCanvasAndGetSizes() {
  CanvasSizes s{};

  double css_w = 0.0, css_h = 0.0;
  emscripten_get_element_css_size("#canvas", &css_w, &css_h);
  s.css_w = (float)css_w;
  s.css_h = (float)css_h;

  const double dpr = emscripten_get_device_pixel_ratio();
  s.fb_w = (int)(css_w * dpr);
  s.fb_h = (int)(css_h * dpr);

  static int last_w = 0, last_h = 0;
  if (s.fb_w != last_w || s.fb_h != last_h) {
    emscripten_set_canvas_element_size("#canvas", s.fb_w, s.fb_h);
    last_w = s.fb_w; last_h = s.fb_h;
  }

  s.scale_x = (s.css_w > 0) ? (float)s.fb_w / s.css_w : 1.0f;
  s.scale_y = (s.css_h > 0) ? (float)s.fb_h / s.css_h : 1.0f;
  return s;
}
static void frame()
{
	glfwPollEvents();

	if (glfwWindowShouldClose(g_window))
	{
		emscripten_cancel_main_loop();
		return;
	}
 const CanvasSizes sz = ResizeCanvasAndGetSizes();
	// Start ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
 ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(sz.css_w, sz.css_h);                 // mouse/UI in CSS pixels
  io.DisplayFramebufferScale = ImVec2(sz.scale_x, sz.scale_y); // renderer to framebuffer
	// Your UI
	ImGui::Begin("Hello ImGui (WebGL1)");
	ImGui::Text("This is ImGui running in the browser.");
	ImGui::ColorEdit4("Clear color", g_clear_color);
	ImGui::Checkbox("Show ImGui demo", &g_show_demo);
	ImGui::End();

	if (g_show_demo)
		ImGui::ShowDemoWindow(&g_show_demo);

	// Render
	ImGui::Render();

  // CSS size (what you see on the page)
 

	
	glViewport(0, 0, sz.fb_w, sz.fb_h);

	glClearColor(g_clear_color[0], g_clear_color[1], g_clear_color[2], g_clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	glfwSwapBuffers(g_window);
} 

int main()
{
	if (!glfwInit())
		return 1;

	// Ask for an OpenGL ES 2.0 context (maps to WebGL1)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	g_window = glfwCreateWindow(1280, 720, "ImGui WebGL", nullptr, nullptr);
	if (!g_window)
		return 2;

	glfwMakeContextCurrent(g_window);
	glfwSwapInterval(1);

	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Init backends
	// For WebGL1/GLES2, use GLSL 100
	ImGui_ImplGlfw_InitForOpenGL(g_window, true);
	ImGui_ImplOpenGL3_Init("#version 100");

	// Main loop
	emscripten_set_main_loop(frame, 0, 1);
	return 0;
}

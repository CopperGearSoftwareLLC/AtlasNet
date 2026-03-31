#pragma once
#include <GL/glew.h>

#include "GLFW/glfw3.h"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
class Window : public Singleton<Window>
{
	GLFWwindow* window = nullptr;

   public:
	Window();

	void PreRender();
	void PostRender();
	void Resize(int width, int height);
	[[nodiscard]] float GetWindowAspect() const
	{
		ASSERT(window, "No Window?");
		ivec2 size;
		glfwGetFramebufferSize(window, &size.x, &size.y);
		return (float)size.x / (float)size.y;
	}
	[[nodiscard]] bool GetShouldClose() const { return glfwWindowShouldClose(window) == 1; }

	vec2 GetMousePosNDC()
	{
		double mouseX, mouseY;
		glfwGetCursorPos(window, &mouseX, &mouseY);

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Normalize to [0, 1]
		float x = static_cast<float>(mouseX) / static_cast<float>(width);
		float y = static_cast<float>(mouseY) / static_cast<float>(height);

		// Convert to [-1, 1] (NDC)
		x = x * 2.0f - 1.0f;
		y = 1.0f - y * 2.0f;  // flip Y

		return vec2(x, y);
	}

   private:
	static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
};
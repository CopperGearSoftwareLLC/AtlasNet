#pragma once

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS


#ifdef _DISPLAY 
#define GLEW_STATIC
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#endif
#include <imgui.h>


#include <iostream>
#include <cstdlib>
#include <thread>
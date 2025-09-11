#pragma once



#include "glm/glm.hpp"
using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
template <glm::length_t L, typename T>
using vec = glm::vec<L, T>;

#ifdef _DISPLAY 
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#define GLEW_STATIC
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#endif

#include "steam/isteamnetworkingsockets.h"
#include "steam/steam_api_common.h"
#include <iostream>
#include <cstdlib>
#include <thread>
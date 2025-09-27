#pragma once


#ifdef _DISPLAY 

/*GLFW + GLEW*/
#define GLEW_STATIC
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>

/*Dear Imgui */
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#endif

/* Valve GameNetworkingSockets */
#include "steam/steamnetworkingsockets.h"

#include "glm/glm.hpp"
using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using quat = glm::quat;
template <glm::length_t L, typename T>
using vec = glm::vec<L, T>;


#include <cassert>
#define ASSERT(check,message) assert(check && message)
#include <iostream>
#include <cstdlib>
#include <thread>
#include <span>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <set>
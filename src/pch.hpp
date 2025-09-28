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
#define STEAMNETWORKINGSOCKETS_STATIC_LINK
#include "steam/steamnetworkingsockets.h"
#include "steam/isteamnetworkingsockets.h"
#include "steam/isteamnetworkingutils.h"
#include "steam/isteamnetworkingmessages.h"
#include "steam/steam_api_common.h"
#include "steam/steamclientpublic.h"
#include "steam/steamnetworkingcustomsignaling.h"
#include "steam/steamnetworkingtypes.h"
#include "steam/steamtypes.h"
#include "steam/steamuniverse.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "nlohmann/json.hpp"
using Json = nlohmann::json;
using Ordered_Json = nlohmann::ordered_json;

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
#include <cstring>
#include <unordered_set>
#include <emmintrin.h> // SSE2
#pragma once


/*GLFW + GLEW*/
#define GLEW_STATIC
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>

/*Dear Imgui */
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui.h>
#include <implot.h>


/* Valve GameNetworkingSockets */
#define STEAMNETWORKINGSOCKETS_STATIC_LINK
#include "GameNetworkingSockets/steam/steamnetworkingsockets.h"
#include "GameNetworkingSockets/steam/isteamnetworkingsockets.h"
#include "GameNetworkingSockets/steam/isteamnetworkingutils.h"
#include "GameNetworkingSockets/steam/isteamnetworkingmessages.h"
#include "GameNetworkingSockets/steam/steam_api_common.h"
#include "GameNetworkingSockets/steam/steamclientpublic.h"
#include "GameNetworkingSockets/steam/steamnetworkingcustomsignaling.h"
#include "GameNetworkingSockets/steam/steamnetworkingtypes.h"
#include "GameNetworkingSockets/steam/steamtypes.h"
#include "GameNetworkingSockets/steam/steamuniverse.h"

#include "curl/curl.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/describe.hpp>
#include <boost/describe/enum.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/describe/enum_from_string.hpp>
#include <boost/stacktrace.hpp>

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
#include <execinfo.h>
#include <cstring>
#include <unordered_set>
#include <emmintrin.h> // SSE2
#include <format>
#include <string>
#include <csignal>
#include <signal.h>
#include <unistd.h>

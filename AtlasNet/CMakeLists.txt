
cmake_minimum_required(VERSION 3.6)

project(AtlasNet LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ATLASNET_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Root directory of AtlasNet")



include (${CMAKE_CURRENT_LIST_DIR}/cmake/Definitions.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/cmake/Install.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/cmake/Deps.cmake)

option(ATLASNET_DEPS_ONLY "Only dependencies" OFF)
if (NOT ATLASNET_DEPS_ONLY)
set(SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/sdk")
#set(DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/deps")

# Global link
link_directories(${CMAKE_SOURCE_DIR}/third_party/lib)

# Recursively find all CMakeLists.txt

    set(NATIVE_JS_OUT ${CMAKE_BINARY_DIR}/js CACHE PATH "native js out") 
option(ATLASNET_INCLUDE_LIBS        "Include Libs" ON)
option(ATLASNET_INCLUDE_API        "Include API"        ON)
option(ATLASNET_INCLUDE_API_JAVA  "Include Java API"   OFF)
option(ATLASNET_INCLUDE_WEB        "Include Web"        OFF)
option(ATLASNET_INCLUDE_RUNTIME    "Include Runtime"    OFF)
option(ATLASNET_INCLUDE_BOOTSTRAP  "Include Bootstrap"  ON)
message(STATUS "ATLASNET_INCLUDE_RUNTIME = ${ATLASNET_INCLUDE_RUNTIME}")

if (ATLASNET_INCLUDE_LIBS)
if (ATLASNET_INCLUDE_RUNTIME)
if (ATLASNET_INCLUDE_WEB)
add_subdirectory(runtime/cartograph)
endif()
add_subdirectory(runtime/proxy)
add_subdirectory(runtime/shard)
#add_subdirectory(runtime/taskworker)
add_subdirectory(runtime/watchdog)
endif()


add_subdirectory(lib/BuiltInDB)
add_subdirectory(lib/Database)
add_subdirectory(lib/Debug)
add_subdirectory(lib/Docker)
add_subdirectory(lib/Entity)
add_subdirectory(lib/Events)
add_subdirectory(lib/Global)
add_subdirectory(lib/Heuristic)
add_subdirectory(lib/Interlink)
add_subdirectory(lib/InternalDB)
if (ATLASNET_INCLUDE_WEB)
include(cmake/SetupNode.cmake)
add_subdirectory(lib/Web)
endif()



if (ATLASNET_INCLUDE_API)
add_subdirectory(API/Client)
add_subdirectory(API/Server)
if (ATLASNET_INCLUDE_API_JAVA)
add_subdirectory(API/Java/Server)
endif()
endif()

if (ATLASNET_INCLUDE_BOOTSTRAP)
add_subdirectory(start/AtlasNet)
endif()
endif()

endif()
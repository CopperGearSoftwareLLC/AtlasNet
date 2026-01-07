cmake_minimum_required(VERSION 3.24)


project(AtlasNet LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include (cmake/Definitions.cmake)
# --- Node.js conditional flag ---
option(ENABLE_NODE "Enable Node.js build for Cartograph" OFF)

# --- SWIG and Node.js setup (conditional) ---
if(ENABLE_NODE)
    message(STATUS "Node.js integration enabled")
    set(NATIVE_JS_OUT ${CMAKE_BINARY_DIR}/js) 
    set(NODE_PROJECT_DIR apps/Cartograph/web)

    execute_process(
        COMMAND node -p "process.execPath"
        OUTPUT_VARIABLE NODE_EXEC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NODE_EXEC STREQUAL "")
        message(FATAL_ERROR "Node.js not found but ENABLE_NODE=ON")
    endif()

    get_filename_component(NODE_BIN_DIR "${NODE_EXEC}" DIRECTORY)
    get_filename_component(NODE_ROOT_DIR "${NODE_BIN_DIR}" DIRECTORY)

    set(NODE_INCLUDE_DIR "${NODE_ROOT_DIR}/include/node")

    if (NOT EXISTS "${NODE_INCLUDE_DIR}/node_api.h")
        message(FATAL_ERROR
            "Node headers not found at ${NODE_INCLUDE_DIR}\n"
            "Make sure Node is installed with development headers"
        )
    endif()

    message(STATUS "Node include dir = ${NODE_INCLUDE_DIR}")

    # Full native build only if Node is enabled
    add_subdirectory(apps/Cartograph) # HAS TO GO FIRST DUE TO NODEJS SWIG REASONS

    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${NODE_PROJECT_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_INCLUDE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\"" "" NODE_ADDON_API_INCLUDE "${NODE_ADDON_API_INCLUDE}")
    include(cmake/AddSwigLibrary.cmake)
    add_subdirectory(libs/Web)

endif()

include (cmake/Install.cmake)

# --- Add other subdirectories unconditionally ---
add_subdirectory(libs/AtlasNet/Server)
add_subdirectory(libs/AtlasNet/Client)
add_subdirectory(libs/AtlasNet/Global)
add_subdirectory(libs/BuiltInDB)
add_subdirectory(libs/Database)
add_subdirectory(libs/Debug)
add_subdirectory(libs/Docker)
add_subdirectory(libs/Entity)
add_subdirectory(libs/Events)
add_subdirectory(libs/Heuristic)
add_subdirectory(libs/Interlink)
add_subdirectory(libs/InternalDB)
add_subdirectory(apps/WatchDog)
add_subdirectory(apps/Proxy)
add_subdirectory(apps/Shard)
add_subdirectory(apps/AtlasNet)



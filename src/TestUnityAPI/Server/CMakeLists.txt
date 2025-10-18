cmake_minimum_required(VERSION 3.20)
project(AtlasUnityBridge LANGUAGES CXX)

# Target platform: Linux x86_64
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# The plugin shared library
add_library(AtlasUnityBridge SHARED
    AtlasUnityBridge.cpp
    AtlasNetServer.cpp
    Partition.cpp
)

# Include all necessary headers
target_include_directories(AtlasUnityBridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Output name (Unity expects "libAtlasUnityBridge.so")
set_target_properties(AtlasUnityBridge PROPERTIES
    OUTPUT_NAME "AtlasUnityBridge"
)

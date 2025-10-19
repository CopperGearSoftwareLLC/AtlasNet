cmake_minimum_required(VERSION 3.20)
project(KDNet LANGUAGES CXX)

# ============================================================
#  Global Build Settings
# ============================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# Root directories
set(SRC_ROOT ${CMAKE_SOURCE_DIR}/src)
set(VCPKG_ROOT ${CMAKE_SOURCE_DIR}/vcpkg_installed)

# ============================================================
#  Platform Detection for vcpkg
# ============================================================
if(WIN32)
    set(VCPKG_INCLUDE_DIR "${VCPKG_ROOT}/x64-windows/include")
    set(VCPKG_LIB_DIR "${VCPKG_ROOT}/x64-windows/lib")
else()
    set(VCPKG_INCLUDE_DIR "${VCPKG_ROOT}/x64-linux/include")
    set(VCPKG_LIB_DIR "${VCPKG_ROOT}/x64-linux/lib")
endif()

message(STATUS "Using VCPKG includes from: ${VCPKG_INCLUDE_DIR}")
message(STATUS "Using VCPKG libs from: ${VCPKG_LIB_DIR}")

# ============================================================
#  Include Directories (mirror Premake)
# ============================================================
include_directories(
    ${SRC_ROOT}
    ${SRC_ROOT}/AtlasNet
    ${SRC_ROOT}/AtlasNet/Server
    ${SRC_ROOT}/AtlasNet/Client
    ${SRC_ROOT}/Partition
    ${SRC_ROOT}/Interlink
    ${SRC_ROOT}/TestUnityAPI/Server
    ${VCPKG_INCLUDE_DIR}
)

# ============================================================
#  Global Defines (mirror Premake)
# ============================================================
add_definitions(
    -D_PORT_GOD=25564
    -D_PORT_PARTITION=25565
    -D_PORT_GAMESERVER=25566
    -DBOOST_STACKTRACE_LINK
    -DBOOST_STACKTRACE_USE_ADDR2LINE
    -DATLAS_FULL_BUILD
)

# ============================================================
#  Source Files
# ============================================================
file(GLOB_RECURSE ALL_SRC_FILES
    "${SRC_ROOT}/*.cpp"
)

if(NOT ALL_SRC_FILES)
    message(FATAL_ERROR "No .cpp files found under ${SRC_ROOT}")
endif()

# ============================================================
#  Unity Plugin Target
# ============================================================
add_library(AtlasUnityBridge SHARED ${ALL_SRC_FILES})
set_target_properties(AtlasUnityBridge PROPERTIES OUTPUT_NAME "AtlasUnityBridge")

# ============================================================
#  Precompiled Header (same behavior as Premake)
# ============================================================
set(PCH_HEADER "${SRC_ROOT}/pch.hpp")
if(EXISTS ${PCH_HEADER})
    message(STATUS "Using precompiled header: ${PCH_HEADER}")
    if(MSVC)
        target_precompile_headers(AtlasUnityBridge PRIVATE ${PCH_HEADER})
    else()
        add_compile_options(-include ${PCH_HEADER})
    endif()
endif()

# ============================================================
#  Auto-Link All vcpkg Libraries
# ============================================================
# This grabs every static (.a) and shared (.so) library in your vcpkg lib folder.
# It ensures that if you install more libs, they'll automatically be linked.
file(GLOB VCPKG_LIBS "${VCPKG_LIB_DIR}/*.a" "${VCPKG_LIB_DIR}/*.so")
if(VCPKG_LIBS)
    message(STATUS "Auto-linking libraries from vcpkg:")
    foreach(lib ${VCPKG_LIBS})
        message(STATUS "  ${lib}")
    endforeach()
endif()

target_link_libraries(AtlasUnityBridge PRIVATE ${VCPKG_LIBS} ssl crypto z dl)

# ============================================================
#  Windows-specific Flags
# ============================================================
if(MSVC)
    target_compile_definitions(AtlasUnityBridge PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ============================================================
#  Post-Build Summary
# ============================================================
message(STATUS "==========================================")
message(STATUS "  AtlasUnityBridge Build Configuration")
message(STATUS "------------------------------------------")
message(STATUS "  Source root : ${SRC_ROOT}")
message(STATUS "  Include dir : ${VCPKG_INCLUDE_DIR}")
message(STATUS "  Library dir : ${VCPKG_LIB_DIR}")
message(STATUS "  Source files: ${ALL_SRC_FILES}")
message(STATUS "==========================================")

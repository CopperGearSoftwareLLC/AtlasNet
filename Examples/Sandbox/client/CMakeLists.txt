cmake_minimum_required(VERSION 3.16)
Set(FETCHCONTENT_QUIET FALSE)
include(FetchContent)
find_package(OpenGL REQUIRED)
# =========================
# GLFW
# =========================
FetchContent_Declare(
    glfw
     URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    GIT_PROGRESS TRUE
)

set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)
# =========================
# GLEW
# =========================
set (GLEW_VERSION "2.1.0")
set (GLEW_URL "https://download.sourceforge.net/sourceforge/project/glew/glew/${GLEW_VERSION}/glew-${GLEW_VERSION}.zip")
set (GLEW_HASH "SHA256=2700383d4de2455f06114fbaf872684f15529d4bdc5cdea69b5fb0e9aa7763f1")

FetchContent_Declare(glew
	URL "${GLEW_URL}"
	URL_HASH "${GLEW_HASH}"
	SOURCE_SUBDIR "build/cmake"
)

FetchContent_MakeAvailable(glew)

FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5.tar.gz
    GIT_PROGRESS TRUE

)

FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    # Backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw glew_s)

# OpenGL (required by imgui_impl_opengl3)

target_link_libraries(imgui PUBLIC OpenGL::GL)
FetchContent_Declare(
    implot
    URL https://github.com/epezent/implot/archive/refs/tags/v0.17.tar.gz
)

FetchContent_MakeAvailable(implot)
add_library(implot STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot PUBLIC
    ${implot_SOURCE_DIR}
)

target_link_libraries(implot PUBLIC imgui)
# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(SandboxClient ${_sources})
target_link_libraries(SandboxClient PUBLIC AtlasNetClient_Static SandboxLib imgui implot)
# Optional, but common: local includes
target_include_directories(SandboxClient
  PRIVATE
  ${glew_SOURCE_DIR}/include
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

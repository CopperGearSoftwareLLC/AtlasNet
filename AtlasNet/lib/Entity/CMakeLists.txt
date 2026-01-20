cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${_target_name} STATIC ${_sources})
# Optional, but common: local includes
target_include_directories(${_target_name}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${NODE_INCLUDE_DIR}
    ${NODE_ADDON_API_INCLUDE}
)


target_link_libraries(${_target_name}
AtlasNetGlobal

)

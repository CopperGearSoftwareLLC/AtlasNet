cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(SandboxLib ${_sources})
target_link_libraries(SandboxLib PRIVATE Native)
# Optional, but common: local includes
target_include_directories(SandboxLib
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
#add_dependencies(SandboxLib install_atlasnet)
cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${_target_name} ${_sources})
target_link_libraries(${_target_name} AtlasNetGlobal Interlink Docker Debug Heuristic Entity)
# Optional, but common: local includes
target_include_directories(${_target_name}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
install(TARGETS ${_target_name}
    RUNTIME DESTINATION bin      # Linux/macOS executables
    COMPONENT ${_target_name}
)

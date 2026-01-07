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
set(GAMENETWORKINGSOCKETS_ROOT "${CMAKE_SOURCE_DIR}/deps/GameNetworkingSockets_install")

add_library(${_target_name} ${_sources})
# Optional, but common: local includes
target_include_directories(${_target_name}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GAMENETWORKINGSOCKETS_ROOT}/include"
)
target_link_libraries(${_target_name}
InternalDB
Docker
    "${GAMENETWORKINGSOCKETS_ROOT}/lib/libGameNetworkingSockets_s.a"
Debug
)

install(TARGETS ${_target_name}
    RUNTIME DESTINATION ${ATLASNET_INSTALL_BIN}
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    )
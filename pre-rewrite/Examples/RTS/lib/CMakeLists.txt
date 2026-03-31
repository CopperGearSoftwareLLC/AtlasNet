cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE _clientsources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/client/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/client/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/client/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/client/*.cpp"
)
# =========================
# Client version of RTSLib
# =========================
add_library(RTSLib_Client ${_sources} ${_clientsources})

target_link_libraries(RTSLib_Client PUBLIC Native imgui implot)

target_include_directories(RTSLib_Client
  PUBLIC
  ${glew_SOURCE_DIR}/include
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/client"
)

target_compile_definitions(RTSLib_Client
  PUBLIC RTS_CLIENT=1 RTS=1
)

# =========================
# Server version of RTSLib
# =========================
add_library(RTSLib_Server ${_sources})

target_link_libraries(RTSLib_Server PUBLIC Native)

target_include_directories(RTSLib_Server
  PUBLIC
  ${glew_SOURCE_DIR}/include
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(RTSLib_Server
  PUBLIC RTS_SERVER=1 RTS=1
)

# Optional
# add_dependencies(RTSLib_Client install_atlasnet)
# add_dependencies(RTSLib_Server install_atlasnet)
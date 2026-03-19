cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(RTSClient ${_sources})
target_link_libraries(RTSClient PUBLIC AtlasNetClient_Static RTSLib_Client )
# Optional, but common: local includes
target_include_directories(RTSClient
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

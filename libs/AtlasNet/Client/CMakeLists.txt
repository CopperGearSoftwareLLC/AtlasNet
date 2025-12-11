cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(AtlasNetClient_Static STATIC ${_sources})
add_library(AtlasNetClient_Shared SHARED ${_sources})
target_link_libraries(AtlasNetClient_Static PUBLIC AtlasNetGeneral)
target_link_libraries(AtlasNetClient_Shared PUBLIC AtlasNetGeneral)
# Optional, but common: local includes
target_include_directories(AtlasNetClient_Static
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_include_directories(AtlasNetClient_Shared
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

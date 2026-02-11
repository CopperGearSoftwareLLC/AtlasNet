cmake_minimum_required(VERSION 3.16)

# ------------------------------
# Collect sources
# ------------------------------
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)



# ------------------------------
# Optional static/shared builds
# ------------------------------



    add_library(AtlasNetClient_Static STATIC ${_sources})
    add_library(AtlasNetClient_Shared SHARED ${_sources})

    target_link_libraries(AtlasNetClient_Static PUBLIC AtlasNetGlobal BuiltInDB Debug Docker Entity Client Interlink)
    target_link_libraries(AtlasNetClient_Shared PUBLIC AtlasNetGlobal BuiltInDB Debug Docker Entity Client Interlink)

target_include_directories(AtlasNetClient_Static
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(AtlasNetClient_Shared
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)



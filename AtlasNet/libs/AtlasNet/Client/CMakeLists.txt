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
# Include directory interface target
# ------------------------------
add_library(AtlasNetClient INTERFACE)
target_include_directories(AtlasNetClient
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

# ------------------------------
# Optional static/shared builds
# ------------------------------
option(BUILD_ATLASNET_CLIENT "Build AtlasNetClient static/shared libraries" ON)

if(BUILD_ATLASNET_CLIENT)
    add_library(AtlasNetClient_Static STATIC ${_sources})
    add_library(AtlasNetClient_Shared SHARED ${_sources})

    target_link_libraries(AtlasNetClient_Static PUBLIC AtlasNetGlobal)
    target_link_libraries(AtlasNetClient_Shared PUBLIC AtlasNetGlobal)

    target_include_directories(AtlasNetClient_Static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(AtlasNetClient_Shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_dependencies(AtlasNetClient AtlasNetClient_Static)
    
endif()

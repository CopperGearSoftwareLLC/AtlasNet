cmake_minimum_required(VERSION 3.16)

# Library target name = folder name (AtlasNetLib)


file(GLOB_RECURSE _lib_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
option(ATLASNETGENERAL_SHARED "Build AtlasNetGeneral as a shared library" OFF)

if(ATLASNETGENERAL_SHARED)
  add_library(AtlasNetGeneral SHARED ${_lib_sources})
  target_compile_definitions(AtlasNetGeneral PRIVATE ATLASNET_BUILD)
else()
  add_library(AtlasNetGeneral STATIC ${_lib_sources})
  set_target_properties(AtlasNetGeneral PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(AtlasNetGeneral
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- Precompiled header ---
# CMake will compile this once and reuse it for sources in this target.
# Note: PCH generally applies to CXX; C sources will ignore it.
target_precompile_headers(AtlasNetGeneral
  PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp>"
)
find_package(Boost REQUIRED COMPONENTS container)
find_package(Boost REQUIRED COMPONENTS stacktrace_addr2line)
find_package(Boost REQUIRED COMPONENTS describe)
find_package(CURL REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
target_link_libraries(AtlasNetGeneral
  PUBLIC
    Boost::container
    Boost::stacktrace_addr2line
    Boost::describe
    glm::glm
    CURL::libcurl
    GameNetworkingSockets::GameNetworkingSockets
    nlohmann_json::nlohmann_json
    redis++::redis++_static
     hiredis::hiredis
    ssl crypto z dl
)

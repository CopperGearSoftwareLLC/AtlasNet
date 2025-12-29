cmake_minimum_required(VERSION 3.16)

# Library target name = folder name (AtlasNetLib)


file(GLOB_RECURSE _lib_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
option(ATLASNETGLOBAL_SHARED "Build ATLASNETGLOBAL as a shared library" OFF)

if(ATLASNETGLOBAL_SHARED)
  add_library(AtlasNetGlobal SHARED ${_lib_sources})
  target_compile_definitions(AtlasNetGlobal PRIVATE ATLASNET_BUILD)
else()
  add_library(AtlasNetGlobal STATIC ${_lib_sources})
  set_target_properties(AtlasNetGlobal PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(AtlasNetGlobal
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- Precompiled header ---
# CMake will compile this once and reuse it for sources in this target.
# Note: PCH generally applies to CXX; C sources will ignore it.
target_precompile_headers(AtlasNetGlobal
  PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp>"
)

find_package(Boost REQUIRED COMPONENTS container)
find_package(Boost REQUIRED COMPONENTS stacktrace_addr2line)
find_package(Boost REQUIRED COMPONENTS describe)
find_package(Boost REQUIRED COMPONENTS type_traits)
find_package(Boost REQUIRED COMPONENTS uuid)
find_package(Boost REQUIRED COMPONENTS multi_index)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(AtlasNetGlobal
  PUBLIC
    Boost::container
    Boost::stacktrace_addr2line
    Boost::describe
    Boost::type_traits
    Boost::multi_index
    Boost::uuid
    glm::glm
    nlohmann_json::nlohmann_json
)

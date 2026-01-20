cmake_minimum_required(VERSION 3.16)

#set(TARGET_NAME MyCustomLibName)

# Use user-defined name if provided, otherwise use folder name
if(NOT DEFINED TARGET_NAME)
    get_filename_component(TARGET_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
endif()

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${TARGET_NAME} STATIC ${_sources})

# Optional, but common: local includes
target_include_directories(${TARGET_NAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${NODE_INCLUDE_DIR}
    ${NODE_ADDON_API_INCLUDE}
)


target_link_libraries(${TARGET_NAME}
    AtlasNetGlobal
    Entity
    InternalDB
    Interlink
)

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

add_library(${_target_name} STATIC ${_sources})
# Optional, but common: local includes

target_include_directories(${_target_name}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)
#install(TARGETS ${_target_name}
#    EXPORT AtlasNetTargets
#    ARCHIVE DESTINATION ${ATLASNET_INSTALL_LIB}
#    LIBRARY DESTINATION ${ATLASNET_INSTALL_LIB}
#    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
#)


target_link_libraries(${_target_name}
Database
Docker
)

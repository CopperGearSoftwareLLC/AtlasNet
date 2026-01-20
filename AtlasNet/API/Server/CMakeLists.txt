cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(AtlasNetServer_Static STATIC ${_sources})
add_library(AtlasNetServer_Shared SHARED ${_sources})
target_link_libraries(AtlasNetServer_Static PUBLIC AtlasNetGlobal BuiltInDB Debug Docker Entity Interlink)
target_link_libraries(AtlasNetServer_Shared PUBLIC AtlasNetGlobal BuiltInDB Debug Docker Entity Interlink)
# Optional, but common: local includes
target_include_directories(AtlasNetServer_Static
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(AtlasNetServer_Shared
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)


set_target_properties(AtlasNetServer_Static PROPERTIES
    OUTPUT_NAME AtlasNetServer_Static
    INSTALL_COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)
set_target_properties(AtlasNetServer_Shared PROPERTIES
    OUTPUT_NAME AtlasNetServer_Shared
    INSTALL_COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)
# install(TARGETS AtlasNetServer_Static AtlasNetServer_Shared
#     EXPORT AtlasNetTargets
#     ARCHIVE DESTINATION ${ATLASNET_INSTALL_LIB}
#     LIBRARY DESTINATION ${ATLASNET_INSTALL_LIB}
#     COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
# )
# install(EXPORT AtlasNetTargets
#     NAMESPACE AtlasNet::
#     DESTINATION ${ATLASNET_INSTALL_LIB}/cmake
#     COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
# )
#add_subdirectory(java)

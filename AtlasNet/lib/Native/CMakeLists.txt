cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(${_target_name} STATIC ${_sources})
# Optional, but common: local includes

target_include_directories(${_target_name}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
#install(TARGETS ${_target_name}
#    EXPORT AtlasNetTargets
#    ARCHIVE DESTINATION ${ATLASNET_INSTALL_LIB}
#    LIBRARY DESTINATION ${ATLASNET_INSTALL_LIB}
#    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
#)


target_link_libraries(${_target_name}
atlasnet_defs
GameNetworkingSockets::GameNetworkingSockets_s
OpenSSL::SSL
OpenSSL::Crypto
redis++::redis++_static
hiredis::hiredis_ssl
hiredis::hiredis
Boost::stacktrace_addr2line
CURL::libcurl
$<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
)

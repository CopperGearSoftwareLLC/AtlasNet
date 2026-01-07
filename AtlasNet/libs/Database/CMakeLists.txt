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

add_library(${_target_name} ${_sources})
# Optional, but common: local includes
target_include_directories(${_target_name}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(REDIS_PLUS_PLUS_ROOT "${CMAKE_SOURCE_DIR}/deps/redis-plus-plus_install")
set(HIREDIS_ROOT "${CMAKE_SOURCE_DIR}/deps/hiredis_install")  # hiredis is inside the same install

# Add include directories
target_include_directories(${_target_name} PUBLIC
    "${REDIS_PLUS_PLUS_ROOT}/include"
    "${HIREDIS_ROOT}/include"
)
message(STATUS "REDIS_PLUS_PLUS_ROOT = ${REDIS_PLUS_PLUS_ROOT}")
# Add library directories
link_directories("${REDIS_PLUS_PLUS_ROOT}/lib")
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
# Manually link the static libraries
target_link_libraries(${_target_name}
    "${REDIS_PLUS_PLUS_ROOT}/lib/libredis++.a"
    "${HIREDIS_ROOT}/lib/libhiredis_ssld.a"
    "${HIREDIS_ROOT}/lib/libhiredisd.a"
    ${LIBUV_LIBRARIES} 
    AtlasNetGlobal
    OpenSSL::SSL
    OpenSSL::Crypto
    ${Protobuf_LIBRARIES}
)

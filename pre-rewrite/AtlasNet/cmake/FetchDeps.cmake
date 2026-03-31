include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Fetching GLM")
SET(GLM_BUILD_TESTS OFF)
FetchContent_Declare(
  glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(glm)

# --- Fetch hiredis ---
find_package(OpenSSL REQUIRED)
message(STATUS "Fetching Redis-Plus-Plus")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Hiredis as static" FORCE)
FetchContent_Declare(
  hiredis
  URL https://github.com/redis/hiredis/archive/refs/tags/v1.3.0.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
  SOURCE_DIR _deps/hiredis
  OVERRIDE_FIND_PACKAGE
)
#FetchContent_MakeAvailable(hiredis)


set(REDIS_PLUS_PLUS_BUILD_STATIC ON CACHE BOOL "Build Redis-Plus-Plus static" FORCE)
set(REDIS_PLUS_PLUS_BUILD_ASYNC "libuv")
FetchContent_Declare(
  redis-plus-plus
  URL https://github.com/sewenew/redis-plus-plus/archive/refs/tags/1.3.15.tar.gz
  USES_TERMINAL_DOWNLOAD TRUE
  DOWNLOAD_NO_EXTRACT FALSE
)
FetchContent_MakeAvailable(redis-plus-plus)


# Only populate if not already done

FetchContent_Declare(
    Protobuf
    URL https://github.com/protocolbuffers/protobuf/archive/refs/tags/v34.0.tar.gz
)
FetchContent_GetProperties(Protobuf)
if(NOT Protobuf_POPULATED)
    FetchContent_Populate(Protobuf)
    add_subdirectory(${protobuf_SOURCE_DIR} ${protobuf_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Make sure GameNetworkingSockets sees it
set(Protobuf_INCLUDE_DIR ${protobuf_SOURCE_DIR}/src CACHE PATH "Protobuf include dir")
set(Protobuf_LIBRARIES protobuf::libprotobuf CACHE STRING "Protobuf library")
set(Protobuf_PROTOC_EXECUTABLE ${protobuf_BINARY_DIR}/protoc CACHE FILEPATH "Protoc executable")
set(Protobuf_USE_STATIC_LIBS ON CACHE BOOL "")

# Now FetchContent GNS
message(STATUS "Fetching GameNetworkingSockets")
FetchContent_Declare(
    GameNetworkingSockets
    URL https://github.com/ValveSoftware/GameNetworkingSockets/archive/refs/tags/v1.4.1.tar.gz
    USES_TERMINAL_DOWNLOAD TRUE
    DOWNLOAD_NO_EXTRACT FALSE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build GameNetworkingSockets as static" FORCE)
FetchContent_MakeAvailable(GameNetworkingSockets)

message(STATUS "Fetching Boost")
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "Use static Boost libraries" FORCE)
set(BOOST_INCLUDE_LIBRARIES bimap describe dynamic_bitset flyweight math multi_array multi_index stacktrace static_string uuid)
set(BOOST_ENABLE_MPI ON)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
        Boost
        URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz
        USES_TERMINAL_DOWNLOAD TRUE
        DOWNLOAD_NO_EXTRACT FALSE
      )
FetchContent_MakeAvailable(Boost)


message(STATUS "Nlohmann Json")
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)




# PCRE2
FetchContent_Declare(
  PCRE2
  URL https://github.com/PCRE2Project/pcre2/archive/refs/tags/pcre2-10.47.tar.gz
)
FetchContent_GetProperties(PCRE2)
if(NOT PCRE2_POPULATED)
    FetchContent_Populate(PCRE2)

    # Build PCRE2 as a static library
    add_subdirectory(${pcre2_SOURCE_DIR} ${pcre2_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Make SWIG aware of PCRE2
set(PCRE2_INCLUDE_DIR ${pcre2_SOURCE_DIR}/src)
set(PCRE2_LIBRARY pcre2-8)


FetchContent_Declare(
  BISON
  URL https://github.com/akimd/bison/archive/refs/tags/v3.8.2.tar.gz
)
FetchContent_GetProperties(BISON)
if(NOT BISON_POPULATED)
    FetchContent_Populate(BISON)

    add_subdirectory(${bison_SOURCE_DIR} ${bison_BINARY_DIR})

    # After building, BISON_EXECUTABLE points to the built bison
    set(BISON_EXECUTABLE "${bison_BINARY_DIR}/src/bison" CACHE FILEPATH "Bison executable")
endif()

# SWIG
FetchContent_Declare(
  SWIG
  URL https://github.com/swig/swig/archive/refs/tags/v4.4.1.tar.gz
)
FetchContent_GetProperties(SWIG)
if(NOT SWIG_POPULATED)
    FetchContent_Populate(SWIG)

    # Build SWIG
    add_subdirectory(${swig_SOURCE_DIR} ${swig_BINARY_DIR})
endif()

# Tell CMake where the executable is
set(SWIG_EXECUTABLE "${swig_BINARY_DIR}/swig" CACHE FILEPATH "Path to SWIG executable")
set(SWIG_DIR "${swig_SOURCE_DIR}" CACHE FILEPATH "Path to SWIG")

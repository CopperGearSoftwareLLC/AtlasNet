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

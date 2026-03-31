cmake_minimum_required(VERSION 3.10)

project(AtlasNet)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 ")
include(cmake/Deps.cmake)

set(ATLASNET_EXAMPLES ON CACHE BOOL "Build AtlasNet examples")
set(ATLASNET_TESTS ON CACHE BOOL "Build AtlasNet tests")
set(ATLASNET_DEV ON CACHE BOOL "Build AtlasNet dev")
set(ATLASNET_WEB OFF CACHE BOOL "Build AtlasNet web interface")

if (ATLASNET_EXAMPLES)
    add_subdirectory(examples)
endif()

if (ATLASNET_TESTS)
enable_testing()
    add_subdirectory(tests)
endif()

if (ATLASNET_DEV)
    add_subdirectory(dev)
endif()


# Recursively collect sources under ./src
file(GLOB_RECURSE _core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)

add_library(AtlasNetCore STATIC ${_core_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetCore
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)
target_link_libraries(AtlasNetCore
GameNetworkingSockets::GameNetworkingSockets_s
OpenSSL::SSL
OpenSSL::Crypto
redis++::redis++_static
hiredis::hiredis
Boost::headers
Boost::beast
Boost::bimap
Boost::describe
Boost::dynamic_bitset
Boost::flyweight
Boost::multi_array
Boost::multi_index
Boost::static_string
Boost::uuid
glm::glm
Taskflow::Taskflow

)



file(GLOB_RECURSE _proxy_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/proxy/*.cpp"
)

add_executable(AtlasNetProxy ${_proxy_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetProxy
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/proxy"
)
add_dependencies(AtlasNetProxy AtlasNetCore)



file(GLOB_RECURSE _watchdog_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/watchdog/*.cpp"
)

add_executable(AtlasNetWatchdog ${_watchdog_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetWatchdog
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/watchdog"
)
add_dependencies(AtlasNetWatchdog AtlasNetCore)


if (ATLASNET_WEB)
include(cmake/WebDeps.cmake)
    add_subdirectory(web)

file(GLOB_RECURSE _web_backend_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/web/backend/*.cpp"
)

add_executable(AtlasNetWebBackend ${_web_backend_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetWebBackend
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/web/backend"
)
target_link_libraries(AtlasNetWebBackend
AtlasNetCore
drogon
)
endif()

file(GLOB_RECURSE _API_shard_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/API/shard/*.cpp"
)
add_library(AtlasNetShardAPI STATIC ${_API_shard_sources})
add_library(AtlasNetShardAPI_s SHARED ${_API_shard_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetShardAPI
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/shard"
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(AtlasNetShardAPI_s
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/shard"
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
add_dependencies(AtlasNetShardAPI AtlasNetCore)
add_dependencies(AtlasNetShardAPI_s AtlasNetCore)

file(GLOB_RECURSE _API_client_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/API/client/*.cpp"
)
add_library(AtlasNetClientAPI_s STATIC ${_API_client_sources})
add_library(AtlasNetClientAPI SHARED ${_API_client_sources})
# Optional, but common: local includes
target_include_directories(AtlasNetClientAPI
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/client"
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/atlasnet/API/client"
)
target_include_directories(AtlasNetClientAPI_s
      PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/API/client"
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/atlasnet/API/client"
)
add_dependencies(AtlasNetClientAPI AtlasNetCore)
add_dependencies(AtlasNetClientAPI_s AtlasNetCore)
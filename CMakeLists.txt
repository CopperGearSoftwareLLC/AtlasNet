cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file")
  endif()
endif()

# Always set install dir (do not hide behind the toolchain fallback)
# Use build dir, not "sourceDir"
set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed"
    CACHE PATH "vcpkg installed dir")

# If you want manifest features, also cache it (or use vcpkg.json)
set(VCPKG_MANIFEST_FEATURES "native"
    CACHE STRING "vcpkg manifest features")

project(AtlasNet LANGUAGES C CXX)

# --- Global settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_definitions(
  _PORT_WATCHDOG=25564
  _PORT_SHARD=25565
  _PORT_GAMESERVER=25566
  _PORT_PROXY=25568

  #Docker Settings
  _DOCKER_OS_="ubuntu:24.04"
  _DOCKER_WORKDIR_="/atlasnet"
  _ATLASNET_STACK_NAME="atlasnet"
  _SWARM_DEFAULT_PORT=2377
  _DOCKER_TEMP_FILES_DIR="temp/docker/"
  _ATLASNET_NETWORK_NAME="AtlasNet"
  _REGISTRY_SERVICE_NAME="registry"
  _REGISTRY_PORT=5000

  #InternalDB
  _INTERNAL_REDIS_SERVICE_NAME="InternalDB"
  #Shard
  _SHARD_SERVICE_NAME="Shard"
  _SHARD_IMAGE_NAME="shard"

  #WatchDog
  _WATCHDOG_IMAGE_NAME="watchdog"
  _WATCHDOG_SERVICE_NAME="WatchDog"

  #Proxy
  _PROXY_IMAGE_NAME="proxy"
  _PROXY_SERVICE_NAME="Proxy"

  #Cartograph
  _CARTOGRAPH_IMAGE_NAME="cartograph"
  _CARTOGRAPH_SERVICE_NAME="Cartograph"

  #Game Server
  _GAME_SERVER_IMAGE_NAME="shard_game_server"

  #Registry
  _REGISTRY_CERT_SECRET_NAME="registry_tls_cert"
  _REGISTRY_KEY_SECRET_NAME="registry_tls_key"
  _REGISTRY_PORT=5000

  # Builder
  _BUILDER_CONTAINER_NAME="atlasnet_builder"
)

# --- Dependencies via vcpkg ---
# find_package(fmt CONFIG REQUIRED)
# find_package(spdlog CONFIG REQUIRED)
# find_package(asio CONFIG REQUIRED)
# find_package(nlohmann_json CONFIG REQUIRED)

# --- Add subprojects ---



if(ATLASNET_BUILD_WASM)
  # Only include wasm-safe libs + Cartograph
  #add_subdirectory(libs/AtlasNet/General)   # or whatever subset you can compile to wasm
  add_subdirectory(apps/Cartograph)
else()

# Full native build
  add_subdirectory(libs/AtlasNet/Server)
  add_subdirectory(libs/AtlasNet/Client)
  add_subdirectory(libs/AtlasNet/Global)
  add_subdirectory(libs/BuiltInDB)
  add_subdirectory(libs/Database)
  add_subdirectory(libs/Debug)
  add_subdirectory(libs/Docker)
  add_subdirectory(libs/Heuristic)
  add_subdirectory(libs/Interlink)
  add_subdirectory(libs/InternalDB)
  add_subdirectory(apps/WatchDog)
  add_subdirectory(apps/Proxy)
  add_subdirectory(apps/Shard)
  add_subdirectory(apps/Bootstrap)
  foreach(_dir IN ITEMS
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/Sandbox/lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/Sandbox/client"
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/Sandbox/server"
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/GameCoordinator_Sample"
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/GameServer_Unity_Sample"
  "${CMAKE_CURRENT_SOURCE_DIR}/dev/Minecraft-fabric-neoforge"
)
  if(IS_DIRECTORY "${_dir}")
    # add_subdirectory expects a path relative to *this* CMakeLists, so pass both:
    file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_dir}")
    add_subdirectory("${_rel}")
  endif()
endforeach()
endif()


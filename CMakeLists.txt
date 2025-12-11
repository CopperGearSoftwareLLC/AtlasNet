cmake_minimum_required(VERSION 3.24)

# --- vcpkg toolchain (must be set BEFORE project()) ---
# Prefer: -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
# Fallback: read it from environment variable VCPKG_ROOT
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain file")
  endif()
endif()

project(AtlasNet LANGUAGES C CXX)

# --- Global settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_definitions(
  _PORT_GOD=25564
  _PORT_PARTITION=25565
  _PORT_GAMESERVER=25566
  _PORT_GAMECOORDINATOR=25567
  _PORT_DEMIGOD=25568

  #Docker Settings
  _DOCKER_OS_="ubuntu:24.04"
  _DOCKER_WORKDIR_="/atlasnet"
  _ATLASNET_STACK_NAME="atlasnet"
  _SWARM_DEFAULT_PORT=2377
  _DOCKER_TEMP_FILES_DIR="temp/docker/"
  _ATLASNET_NETWORK_NAME="AtlasNet"
  _REGISTRY_SERVICE_NAME="registry"
  _REGISTRY_PORT=5000
  #Cluster Database
  _CLUSTERDB_IMAGE_NAME="cluster-db"
  _CLUSTERDB_SERVICE_NAME="ClusterDB"

  #REDIS
  _REDIS_SERVICE_NAME="redis"
  #Partition
  _PARTITION_SERVICE_NAME="Partition_GameServer"
  _PARTITION_IMAGE_NAME="partition"

  #God
  _GOD_IMAGE_NAME="god"
  _GOD_SERVICE_NAME="God"

  #DemiGod
  _DEMIGOD_IMAGE_NAME="demi-god"
  _DEMIGOD_SERVICE_NAME="DemiGod"


  #Game Coordinator
  _GAME_COORDINATOR_IMAGE_NAME="game-coordinator"
  _GAME_COORDINATOR_SERVICE_NAME="GameCoordinator"
  _GAME_COORDINATOR_PORT=25567

  #Game Server
  _GAME_SERVER_IMAGE_NAME="game-server"

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
add_subdirectory(libs/AtlasNet/General)
add_subdirectory(libs/AtlasNet/Client)
add_subdirectory(libs/AtlasNet/Server)

add_subdirectory(libs/ImguiAppBase)

add_subdirectory(apps/God)
add_subdirectory(apps/DemiGod)
add_subdirectory(apps/Cartograph)
add_subdirectory(apps/Partition)
add_subdirectory(apps/ClusterDB)
add_subdirectory(apps/Launcher)
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
cmake_minimum_required(VERSION 3.10)

# Recursively collect sources under ./src
file(GLOB_RECURSE _core_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)

add_library(TankBattleCore ${_core_sources})
# Optional, but common: local includes
target_include_directories(TankBattleCore
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)


# Recursively collect sources under ./src
file(GLOB_RECURSE _shard_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/shard/*.cpp"
)

add_library(TankBattleShard ${_shard_sources})
# Optional, but common: local includes
target_include_directories(TankBattleShard
      PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shard"
)

target_link_libraries(TankBattleShard TankBattleCore AtlasNetShardAPI_s)



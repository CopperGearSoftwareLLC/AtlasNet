cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# ------------------------------------------------------------------------------
# Paths (AtlasNet-mc layout)
# ------------------------------------------------------------------------------
set(_mc_root     "${CMAKE_CURRENT_LIST_DIR}")
set(_gradle_root "${_mc_root}/src")

# Where AtlasNetServer_Java outputs should be staged for the Gradle project
set(_stage_jars    "${_gradle_root}/.atlasnet")  # jars go here
set(_stage_natives "${_mc_root}/natives")        # .so go here

# Where final built mod jars (fabric/neoforge) should be copied
set(_out_mods "${_mc_root}/build")

# Keep Gradle cache out of src/
set(_gradle_user_home "${_mc_root}/build/.gradle")

# ------------------------------------------------------------------------------
# Producer outputs (from AtlasNetServer_Java target)
# ------------------------------------------------------------------------------
set(_dist_dir
  "$<TARGET_PROPERTY:AtlasNetServer_Java,ATLASNETSERVER_JAVA_DIST_DIR>"
)
set(_dist_jar
  "$<TARGET_PROPERTY:AtlasNetServer_Java,ATLASNETSERVER_JAVA_DIST_JAR>"
)

# ------------------------------------------------------------------------------
# Stage AtlasNetServer_Java artifacts
# ------------------------------------------------------------------------------
# Track staging with a real OUTPUT so Ninja can reason about it
set(_stage_stamp
  "${CMAKE_CURRENT_BINARY_DIR}/${_target_name}_atlasnetserver_java.stage.stamp"
)

add_custom_command(
  OUTPUT "${_stage_stamp}"

  # Clean + recreate staging dirs to avoid stale files
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${_stage_jars}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${_stage_natives}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_jars}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_stage_natives}"

  # Copy jar into src/.atlasnet/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${_dist_jar}"
          "${_stage_jars}/"

  # Copy natives (.so) into natives/
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${_dist_dir}/natives"
          "${_stage_natives}"

  # Stamp
  COMMAND ${CMAKE_COMMAND} -E touch "${_stage_stamp}"

  # CRITICAL:
  #  - depend on the Java target
  #  - depend on the actual jar artifact
  DEPENDS
    AtlasNetServer_Java_Dist
    "${_dist_jar}"

  VERBATIM
  USES_TERMINAL
)

add_custom_target(${_target_name}_stage_atlasnetserver_java
  DEPENDS "${_stage_stamp}"
)

# ------------------------------------------------------------------------------
# Script to collect Fabric + NeoForge jars after Gradle build
# ------------------------------------------------------------------------------
set(_collect_script
  "${CMAKE_CURRENT_BINARY_DIR}/${_target_name}_collect_mods.cmake"
)

file(WRITE "${_collect_script}" [=[
if(NOT DEFINED GRADLE_ROOT)
  message(FATAL_ERROR "GRADLE_ROOT not set")
endif()

if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR not set")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

file(GLOB_RECURSE _fabric_jars
  "${GRADLE_ROOT}/build/libs/*fabric*.jar"
  "${GRADLE_ROOT}/*/build/libs/*fabric*.jar"
)

file(GLOB_RECURSE _neoforge_jars
  "${GRADLE_ROOT}/build/libs/*neoforge*.jar"
  "${GRADLE_ROOT}/*/build/libs/*neoforge*.jar"
  "${GRADLE_ROOT}/build/libs/*forge*.jar"
  "${GRADLE_ROOT}/*/build/libs/*forge*.jar"
)

set(_all_jars ${_fabric_jars} ${_neoforge_jars})

if(_all_jars STREQUAL "")
  message(FATAL_ERROR
    "No Fabric/NeoForge jars found under ${GRADLE_ROOT}"
  )
endif()

foreach(j IN LISTS _all_jars)
  message(STATUS "Copying mod jar: ${j}")
  file(COPY "${j}" DESTINATION "${OUT_DIR}")
endforeach()
]=])

# ------------------------------------------------------------------------------
# Gradle build
# ------------------------------------------------------------------------------
add_custom_target(${_target_name}_gradle_build
  COMMAND ${CMAKE_COMMAND} -E env
          GRADLE_USER_HOME="${_gradle_user_home}"
          "${_gradle_root}/gradlew"
          --console=auto
          --info
          build
  WORKING_DIRECTORY "${_gradle_root}"
  DEPENDS ${_target_name}_stage_atlasnetserver_java
  USES_TERMINAL
)

# ------------------------------------------------------------------------------
# Collect mod jars
# ------------------------------------------------------------------------------
add_custom_target(${_target_name}_collect_mods
  COMMAND ${CMAKE_COMMAND}
          -DGRADLE_ROOT="${_gradle_root}"
          -DOUT_DIR="${_out_mods}"
          -P "${_collect_script}"
  DEPENDS ${_target_name}_gradle_build
  USES_TERMINAL
)

# ------------------------------------------------------------------------------
# Main target
# ------------------------------------------------------------------------------
add_custom_target(${_target_name} ALL
  DEPENDS ${_target_name}_collect_mods
)

# ------------------------------------------------------------------------------
# Launch task
# ------------------------------------------------------------------------------
add_custom_target(${_target_name}_launch
  COMMAND ${CMAKE_COMMAND} -E env
          DOCKER_BUILDKIT=1
          ${CMAKE_COMMAND} -E chdir "$<TARGET_FILE_DIR:Bootstrap>"
          "$<TARGET_FILE:Bootstrap>"
          -f "${CMAKE_CURRENT_SOURCE_DIR}/AtlasNetSettings.json"
  DEPENDS
          stage_Bootstrap
          ${_target_name}
          Bootstrap
  USES_TERMINAL
  VERBATIM
)

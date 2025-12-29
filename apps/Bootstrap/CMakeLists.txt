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
set(BOOTSTRAP_RUNTIME_SRC_DIR ".Runtime")  # CMake variable

add_executable(${_target_name} ${_sources})
target_link_libraries(${_target_name} AtlasNetGlobal Debug Docker)
# Optional, but common: local includes
target_include_directories(${_target_name}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(${_target_name} PRIVATE
  BOOTSTRAP_SETTINGS_FILE_FLAG="-f"

  BOOTSTRAP_RUNTIME_SRC_DIR="${BOOTSTRAP_RUNTIME_SRC_DIR}"  # macro for C++ if you want it
)

install(TARGETS ${_target_name}
    RUNTIME DESTINATION bin      # Linux/macOS executables
)
# --- Stage runtime folders next to the built executable on every build ---

# "root_cmake" in your wording = the top-level project root (since this is add_subdirectory()).
set(ATLASNET_ROOT_CMAKE_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH
    "Top-level source dir containing apps/ and libs/")

# Copy into the folder where the executable is produced.
# (Works even if the parent sets RUNTIME_OUTPUT_DIRECTORY, multi-config generators, etc.)
# ---- Always-run staging step (runs on every build) ----

set(_stage_stamp "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/stage_${_target_name}.stamp")

add_custom_command(
  OUTPUT "${_stage_stamp}"

  COMMAND ${CMAKE_COMMAND} -E echo
          "Staging apps/ and libs/ next to $<TARGET_FILE_NAME:${_target_name}>"

  # "clean" the staged folders (NOT the actual build tree)
  COMMAND ${CMAKE_COMMAND} -E rm -rf
          "$<TARGET_FILE_DIR:${_target_name}>/${BOOTSTRAP_RUNTIME_SRC_DIR}"

  # copy from root_cmake/apps + root_cmake/libs
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ATLASNET_ROOT_CMAKE_DIR}/apps"
          "$<TARGET_FILE_DIR:${_target_name}>/${BOOTSTRAP_RUNTIME_SRC_DIR}/apps"

  COMMAND ${CMAKE_COMMAND} -E copy
          "${ATLASNET_ROOT_CMAKE_DIR}/vcpkg.json"
          "$<TARGET_FILE_DIR:${_target_name}>/${BOOTSTRAP_RUNTIME_SRC_DIR}"

  COMMAND ${CMAKE_COMMAND} -E copy
          "${ATLASNET_ROOT_CMAKE_DIR}/CMakeLists.txt"
          "$<TARGET_FILE_DIR:${_target_name}>/${BOOTSTRAP_RUNTIME_SRC_DIR}"

  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ATLASNET_ROOT_CMAKE_DIR}/libs"
          "$<TARGET_FILE_DIR:${_target_name}>/${BOOTSTRAP_RUNTIME_SRC_DIR}/libs"

  # Make sure the executable exists first:
  DEPENDS "$<TARGET_FILE:${_target_name}>"

  VERBATIM
)

# Critical bit: make this OUTPUT "symbolic" so it is always treated as out-of-date.
set_source_files_properties("${_stage_stamp}" PROPERTIES SYMBOLIC TRUE)

# Custom target that runs on every normal build (because it's in ALL).
add_custom_target(stage_${_target_name} ALL
  DEPENDS "${_stage_stamp}"
)

# Also ensure build order in IDEs / multi-config generators.
add_dependencies(stage_${_target_name} ${_target_name})

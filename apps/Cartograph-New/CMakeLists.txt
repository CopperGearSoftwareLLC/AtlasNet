cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Recursively collect sources under ./src
file(GLOB_RECURSE viewer_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
set(Cartograph_binary ${_target_name}_viewer)
add_executable(Cartograph_binary ${viewer_sources})
target_link_libraries(Cartograph_binary AtlasNetGeneral)
# Optional, but common: local includes
target_include_directories(Cartograph_binary
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  message(STATUS "Configuring Cartograph_binary for Emscripten/WebAssembly")

    # Place outputs in a nice dist/ folder
    set_target_properties(Cartograph_binary PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
        OUTPUT_NAME "Cartograph_binary"
    )
    target_link_options(Cartograph_binary PRIVATE
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASSERTIONS=1"
        "-sDISABLE_EXCEPTION_CATCHING=0"
        # Enable WebGPU when you wire it up:
        # "-sUSE_WEBGPU=1"

        # Export runtime helpers so JS/TS can call into WASM
        "-sEXPORTED_RUNTIME_METHODS=['cwrap','ccall']"

        # Let Emscripten generate a modular JS wrapper
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='CartographViewerModule'"

        # Keep the HTML minimal (you may not even need HTML if Next.js loads the .js/.wasm directly)
        "--no-entry"
    )
else()
    message(STATUS "Configuring atlasnet_viewer as a native binary")

    # Native build settings (useful for local debugging of the renderer)
    if (MSVC)
        target_compile_options(Cartograph_binary PRIVATE /W4)
    else()
        target_compile_options(Cartograph_binary PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()


set(CARTOGRAPH_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web")
# Allow overriding npm executable (e.g. on Windows you might want npm.cmd)
set(NPM_EXECUTABLE "npm" CACHE STRING "Path to npm executable")

# Optional: a target to ensure node modules are installed
add_custom_target(Cartograph_web_npm_install
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} install
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] npm install"
)
# Main dev target: starts `npm run dev` with hot reload
add_custom_target(Cartograph_web_dev
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} run dev
  USES_TERMINAL
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] Running Next.js dev server (hot reload on :3000)"
)

# Make sure deps are installed before dev (optional but nice)
add_dependencies(Cartograph_web_dev Cartograph_web_npm_install)

register_docker_service(
  NAME      Cartograph
  SERVICE   cartograph
  IMAGE     cartograph:latest
  DOCKERFILE "${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile"
  CONTEXT    "${CMAKE_CURRENT_SOURCE_DIR}"

  # New goodies:
  PORTS "3000:3000"              # You can add more: "80:80" "443:443"
  NETWORKS ${ATLASNET_NETWORK_NAME}      # Or multiple networks

  RESTART_CONDITION "on-failure"   # Swarm restart_policy.condition

  DEPLOY_CONSTRAINTS
    "node.role==manager"
  REPLICAS 1
  # deploy.resources.limits
  RES_LIMIT_CPUS "1.0"
  RES_LIMIT_MEM  "1G"

  # deploy.resources.reservations
  #RES_RES_CPUS   "0.25"
  #RES_RES_MEM    "256M"
)
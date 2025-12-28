cmake_minimum_required(VERSION 3.16)

get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
set(CARTOGRAPH_TARGET "${_target_name}_viewer")

file(GLOB_RECURSE viewer_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${CARTOGRAPH_TARGET} ${viewer_sources})



target_include_directories(${CARTOGRAPH_TARGET}
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(CARTOGRAPH_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(NPM_EXECUTABLE "npm" CACHE STRING "Path to npm executable")

if (ATLASNET_BUILD_WASM)
  message(STATUS "Configuring ${CARTOGRAPH_TARGET} for Emscripten/WebAssembly + WebGPU")

  # Build artifacts go here
  set(WASM_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
  set_target_properties(${CARTOGRAPH_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WASM_DIST_DIR}"
    OUTPUT_NAME "CartographViewer"         # produces CartographViewer.js + CartographViewer.wasm
    SUFFIX ".js"                           # IMPORTANT: emcc outputs JS loader for executables
  )

  # Put the wasm artifacts where Next can serve them:
  set(NEXT_PUBLIC_WASM_DIR "${CARTOGRAPH_WEB_ROOT}/public/wasm")
target_compile_options(${CARTOGRAPH_TARGET} PRIVATE "--use-port=emdawnwebgpu")
  # Core Emscripten/WebGPU flags
  add_compile_definitions(__EMSCRIPTEN__)
  target_link_options(${CARTOGRAPH_TARGET} PRIVATE
    "-sWASM=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sASSERTIONS=1"

    # WebGPU on
    "--use-port=emdawnwebgpu"
    "-sUSE_GLFW=3"
    # Make it friendlier to import from Next/TS
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=CartographViewerModule"
    "-sEXPORT_ES6=1"
    "-sENVIRONMENT=web,worker"

    # If you want to call into WASM from JS:
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"

    # You usually want a real entry point for a renderer:
    # (use main()+emscripten_set_main_loop inside C++)
    # so DO NOT use --no-entry unless you truly want a pure library.
    # "--no-entry"
  )
  find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${CARTOGRAPH_TARGET} PRIVATE glfw)
  # Copy output to Next public/wasm after build
  add_custom_command(TARGET ${CARTOGRAPH_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NEXT_PUBLIC_WASM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:${CARTOGRAPH_TARGET}>"                       "${NEXT_PUBLIC_WASM_DIR}/CartographViewer.js"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${WASM_DIST_DIR}/CartographViewer.wasm"                    "${NEXT_PUBLIC_WASM_DIR}/CartographViewer.wasm"
    COMMENT "[wasm] Copying CartographViewer.{js,wasm} -> web/public/wasm/"
  )

else()
  message(STATUS "Configuring ${CARTOGRAPH_TARGET} as a native binary")

  if (MSVC)
    target_compile_options(${CARTOGRAPH_TARGET} PRIVATE /W4)
  else()
    target_compile_options(${CARTOGRAPH_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
# ---- WASM superbuild target (vcpkg toolchain + chainload emscripten) ----
# Usage:
#   cmake --build build/native --target wasm
# or from an IDE/VS Code: build target "wasm"

set(ATLAS_WASM_BUILD_DIR "${CMAKE_SOURCE_DIR}/build/wasm" CACHE PATH "Out-of-tree build dir for wasm")

# You can change triplet if you use a custom one
set(ATLAS_WASM_TRIPLET "wasm32-emscripten" CACHE STRING "vcpkg triplet for wasm")

# Default generator: reuse current generator if possible, else Ninja
if(CMAKE_GENERATOR)
  set(_WASM_GEN "${CMAKE_GENERATOR}")
else()
  set(_WASM_GEN "Ninja")
endif()
add_custom_target(Cartograph_wasm
  COMMAND ${CMAKE_COMMAND} -E echo "== Configuring preset: wasm"
  COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_SOURCE_DIR}" ${CMAKE_COMMAND} --preset wasm
  COMMAND ${CMAKE_COMMAND} -E echo "== Building buildPreset: wasm"
  COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_SOURCE_DIR}" ${CMAKE_COMMAND} --build --preset wasm
  USES_TERMINAL
  COMMENT "Building WebAssembly using the 'wasm' configure/build presets"
)

# --- Next.js helpers (unchanged, but your -E chdir usage is correct) ---
find_program(NPM_EXECUTABLE npm REQUIRED)
# Optional: also ensure node exists
find_program(NODE_EXECUTABLE node REQUIRED)
add_custom_target(Cartograph_web_npm_install
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} install
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] npm install"
)

add_custom_target(Cartograph_web_dev
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} run dev
  USES_TERMINAL
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] Running Next.js dev server (hot reload on :3000)"
)
add_dependencies(Cartograph_web_dev Cartograph_web_npm_install)
add_dependencies(Cartograph_web_npm_install ${CARTOGRAPH_TARGET})

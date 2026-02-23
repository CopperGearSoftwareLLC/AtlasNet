cmake_minimum_required(VERSION 3.16)

get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(CARTOGRAPH_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web" CACHE PATH "Path to Cartograph web root")

set(NPM_EXECUTABLE "npm" CACHE STRING "Path to npm executable")

# Aggregate target for the web project

# Copy everything to Next.js
set(NEXTJS_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nextjs/native")
message(STATUS "NEXTJS_NATIVE_DIR = '${NEXTJS_NATIVE_DIR}'")
message(STATUS "NATIVE_JS_OUT = '${NATIVE_JS_OUT}'")

# Collect all .node files
file(GLOB NODE_ADDONS
    "${NATIVE_JS_OUT}/*.node"
)


add_custom_target(Cartograph_web_dev
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} run dev:all 
  USES_TERMINAL
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] Running Next.js dev server (hot reload on :3000)"
)
#add_dependencies(Cartograph_web_dev Cartograph_web_npm_install)
#add_dependencies(Cartograph_web_dev cartograph_web_native_deps)
#add_dependencies(Cartograph_web_npm_install ${CARTOGRAPH_TARGET})

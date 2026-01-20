cmake_minimum_required(VERSION 3.16)

get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

set(CARTOGRAPH_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web" CACHE PATH "Path to Cartograph web root")

set(NPM_EXECUTABLE "npm" CACHE STRING "Path to npm executable")

# Aggregate target for the web project

# Copy everything to Next.js
set(NEXTJS_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web/nextjs/native")
message(STATUS "NEXTJS_NATIVE_DIR = '${NEXTJS_NATIVE_DIR}'")
message(STATUS "NATIVE_JS_OUT = '${NATIVE_JS_OUT}'")

# Collect all .node files
file(GLOB NODE_ADDONS
    "${NATIVE_JS_OUT}/*.node"
)

add_custom_target(cartograph_web_native_deps ALL
    DEPENDS
        Web_swig
)

add_custom_command(
    TARGET cartograph_web_native_deps
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${NATIVE_JS_OUT}
            ${NEXTJS_NATIVE_DIR}
    VERBATIM
)

#set(ENV_FILE "${CARTOGRAPH_WEB_ROOT}/.env")
#
#file(WRITE "${ENV_FILE}" "")
#foreach(def IN LISTS GLOBAL_DEFINITIONS)
#    # Split key/value
#    string(REGEX MATCH "^[^=]+" key ${def})
#    string(REGEX REPLACE "^[^=]+=" "" value ${def})
#    
#    # Remove quotes from CMake string if needed
#    string(REGEX REPLACE "^\"|\"$" "" value ${value})
#
#    # Next.js requires env vars starting with NEXT_PUBLIC_ to expose to client-side
#    set(js_key "NEXT_PUBLIC_${key}")
#    file(APPEND "${ENV_FILE}" "${js_key}=${value}\n")
#endforeach()
#message(STATUS "Generated Next.js .env file at ${ENV_FILE}")
# --- Next.js helpers (unchanged, but your -E chdir usage is correct) ---
find_program(NPM_EXECUTABLE npm REQUIRED)
# Optional: also ensure node exists
find_program(NODE_EXECUTABLE node REQUIRED)
execute_process(
    COMMAND ${NPM_EXECUTABLE} install
    WORKING_DIRECTORY ${CARTOGRAPH_WEB_ROOT}
    RESULT_VARIABLE npm_result

)
add_custom_target(Cartograph_web_dev
  COMMAND ${CMAKE_COMMAND} -E chdir "${CARTOGRAPH_WEB_ROOT}" ${NPM_EXECUTABLE} run dev:all 
  USES_TERMINAL
  WORKING_DIRECTORY "${CARTOGRAPH_WEB_ROOT}"
  COMMENT "[web] Running Next.js dev server (hot reload on :3000)"
)
#add_dependencies(Cartograph_web_dev Cartograph_web_npm_install)
add_dependencies(Cartograph_web_dev cartograph_web_native_deps)
#add_dependencies(Cartograph_web_npm_install ${CARTOGRAPH_TARGET})

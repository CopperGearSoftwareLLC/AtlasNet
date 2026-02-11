cmake_minimum_required(VERSION 3.6)

option(SANDBOX_ADD_ATLASNET        "Include AtlasNet" ON)
if ((NOT ATLASNET_ROOT) AND SANDBOX_ADD_ATLASNET)
add_subdirectory(sandbox/.AtlasNet)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SANDBOX_ADD_CLIENT        "Include AtlasNet" OFF)

add_subdirectory(lib)
add_subdirectory(server)
if(SANDBOX_ADD_CLIENT)
add_subdirectory(client)
endif()
#add_run_target(
#    NAME run_atlasnet_sandbox
#    TARGET AtlasNet
#    ARGS -f ${CMAKE_CURRENT_SOURCE_DIR}/AtlasNetSettings.json
#    ENV ATLASNET_ROOT=${ATLASNET_ROOT}
#)
add_custom_target(run_atlasnet_sandbox
        COMMAND
            ${CMAKE_COMMAND} -E env
            ATLASNET_ROOT=${ATLASNET_ROOT}
            $<TARGET_FILE:AtlasNet>
            -f ${CMAKE_CURRENT_SOURCE_DIR}/AtlasNetSettings.json
        DEPENDS AtlasNet
        #WORKING_DIRECTORY ${RUN_WORKING_DIR}
        USES_TERMINAL
    )
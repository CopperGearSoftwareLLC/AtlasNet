cmake_minimum_required(VERSION 3.6)

option(SANDBOX_ADD_ATLASNET        "Include AtlasNet" ON)
if ((NOT ATLASNET_ROOT) AND SANDBOX_ADD_ATLASNET)
add_subdirectory(sandbox/.AtlasNet)
endif()

set(SANDBOX_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Root directory of Sandbox")
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



add_custom_target(sandbox_atlasnet_run_K8s
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dDev.sh atlasnet-dev ${SANDBOX_SERVER_IMAGE_NAME}
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
     USES_TERMINAL
     DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast 
     )

add_custom_target(sandbox_atlasnet_run_Docker
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetDevStack.sh atlasnet_dev ${SANDBOX_SERVER_IMAGE_NAME}
     USES_TERMINAL
     DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast 
     )

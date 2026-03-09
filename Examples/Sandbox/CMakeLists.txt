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



add_custom_target(sandbox_atlasnet_run_k3d
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dDev.sh atlasnet-dev ${SANDBOX_SERVER_IMAGE_NAME}
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
     USES_TERMINAL
     DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast 
     )

add_custom_target(sandbox_atlasnet_run_Docker
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetDevStack.sh atlasnet_dev ${SANDBOX_SERVER_IMAGE_NAME}
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
     USES_TERMINAL
     DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast 
     )

add_custom_target(sandbox_atlasnet_run_k3d_fast
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetK3dDev.sh atlasnet-dev ${SANDBOX_SERVER_IMAGE_NAME}
     USES_TERMINAL
     DEPENDS AtlasnetDockerBuild_FastCached SandboxServerDockerBuild_Fast
     )

# k3s slim command routing (keep this surface area small).
find_program(ATLASNET_MAKE_EXECUTABLE make REQUIRED)
set(ATLASNET_K3S_SLIM_DIR "${CMAKE_SOURCE_DIR}/deploy/k3s_slim")

function(add_k3s_target cmake_target make_target)
  add_custom_target(${cmake_target}
    COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "${make_target}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "k3s: ${make_target}"
    )
    endfunction()
    
# Non-overlapping utility targets.
add_k3s_target(k3s_1_atlasnet_merge_manifests atlasnet-merge-manifests)
add_k3s_target(k3s_5_atlasnet_deploy atlasnet-deploy)
add_k3s_target(k3s_4_node_logs_start node-logs-start)
add_k3s_target(k3s_6_node_logs_stop node-logs-stop)
add_k3s_target(k3s_7_cleanup_cluster cleanup-cluster)

# Consolidated workflows.
add_custom_target(k3s_0_build_and_push
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "atlasnet-push"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  DEPENDS AtlasnetDockerBuild_Fast SandboxServerDockerBuild
  COMMENT "Build images, then push AtlasNet images"
)

add_custom_target(k3s_2_setup_nodes
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "ssh-setup"
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "sudo-setup"
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "dependency-setup"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Run all node setup steps (ssh, sudo, dependency)"
)

add_custom_target(k3s_3_deploy_and_platform
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "k3s-deploy"
  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_SLIM_DIR}" "platform"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  USES_TERMINAL
  COMMENT "Deploy k3s cluster, then install platform add-ons"
)


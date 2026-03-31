cmake_minimum_required(VERSION 3.6)

option(RTS_ADD_ATLASNET        "Include AtlasNet" ON)
if ((NOT ATLASNET_ROOT) AND RTS_ADD_ATLASNET)
add_subdirectory(RTS/.AtlasNet)
endif()

set(RTS_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Root directory of RTS")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



add_subdirectory(lib)
add_subdirectory(server)
add_subdirectory(client)




add_custom_target(RTS_atlasnet_run_k3d
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/k8s/k3d/scripts/FreePortsAndServices.sh
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/k8s/k3d/scripts/RunAtlasNetK3dDev.sh atlasnet-dev ${RTS_SERVER_IMAGE_NAME}
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
     USES_TERMINAL
     DEPENDS RTSServerDockerBuild AtlasnetDockerBuild_Fast 
     )

#DEFINE_CUSTOM_TARGET(RTS_atlasnet_run_Docker_v2 ${RTS_SERVER_IMAGE_NAME})
#DEFINE_CUSTOM_VALGRIND_TARGET(RTS_atlasnet_run_Docker_v2 ${RTS_SERVER_IMAGE_NAME})

#add_custom_target(RTS_atlasnet_run_Docker
#COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetDevStack.sh atlasnet_dev ${RTS_SERVER_IMAGE_NAME}
##COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/PruneUnusedDockerResources.sh
#     USES_TERMINAL
#     DEPENDS RTSServerDockerBuild AtlasnetDockerBuild_Fast 
#     )
add_custom_target(RTS_atlasnet_run_Docker
    COMMAND echo "Removing existing stack (if any)..."
    COMMAND docker stack rm RTS || true

    COMMAND ${CMAKE_COMMAND} -E echo "Waiting for stack to be removed..."
    COMMAND bash -c "while docker stack ls --format '{{.Name}}' | grep -w RTS >/dev/null 2>&1; do sleep 1; done"

    COMMAND echo "Deploying stack..."
    COMMAND docker stack deploy -c ${CMAKE_CURRENT_SOURCE_DIR}/RTSDockerStack.yml RTS

    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
    DEPENDS RTSServerDockerBuild AtlasnetDockerBuild_Fast 
)
add_custom_target(RTS_atlasnet_apply_latency
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/LocalRuntimeLatency.sh auto apply-auto
     USES_TERMINAL
     COMMENT "Local runtime: apply latency to a random auto-detected target"
     )

add_custom_target(RTS_atlasnet_clear_latency
     COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} bash ${CMAKE_SOURCE_DIR}/Dev/LocalRuntimeLatency.sh auto clear-all
     USES_TERMINAL
     COMMENT "Local runtime: clear latency from the auto-detected target set"
     )

# k3s command routing (keep this surface area small).
find_program(ATLASNET_MAKE_EXECUTABLE make REQUIRED)
#set(ATLASNET_K3S_DIR "${CMAKE_SOURCE_DIR}/k8s/k3s")

#function(add_k3s_target cmake_target make_target)
#  add_custom_target(${cmake_target}
#    COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "${make_target}"
#    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#    USES_TERMINAL
#    COMMENT "k3s: ${make_target}"
#  )
#endfunction()
    
# Non-overlapping utility targets.
#add_k3s_target(k3s_03_cleanup_cluster cleanup-cluster)
#add_k3s_target(k3s_04_node_logs_start node-logs-start)
#add_k3s_target(k3s_05_node_logs_stop node-logs-stop)
#add_k3s_target(k3s_06_latency_apply_auto_worker latency-apply-auto-worker)
#add_k3s_target(k3s_07_latency_clear_all latency-clear-all)

## Consolidated workflows.
#add_custom_target(k3s_00_build_push_merge
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "atlasnet-push"
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "atlasnet-merge-manifests"
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  USES_TERMINAL
#  DEPENDS AtlasnetDockerBuild_Fast RTSServerDockerBuild
#  COMMENT "Build images, push AtlasNet images, then merge manifests"
#)
#
#add_custom_target(k3s_01_ssh_readyup_nodes
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "ssh-setup"
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "sudo-setup"
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "dependency-setup"
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  USES_TERMINAL
#  COMMENT "Run all node setup steps (ssh, sudo, dependency)"
#)
#
#add_custom_target(k3s_02_cluster_and_deploy
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "k3s-deploy"
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "platform"
#  COMMAND "${ATLASNET_MAKE_EXECUTABLE}" -C "${ATLASNET_K3S_DIR}" "atlasnet-deploy"
#  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
#  USES_TERMINAL
#  COMMENT "Deploy k3s cluster, install platform add-ons, then deploy AtlasNet"
#)

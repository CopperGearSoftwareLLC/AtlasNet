


add_custom_target(AtlasnetDockerBuild_Fast_Stage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ATLASNET_ROOT}/.stage
    #COMMAND ${CMAKE_COMMAND} -E copy
    #        $<TARGET_FILE:AtlasNetServer_Shared>
    #        ${SANDBOX_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:watchdog>
            ${ATLASNET_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:shard>
            ${ATLASNET_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:proxy>
            ${ATLASNET_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ATLASNET_ROOT}/.stage/cartograph
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ATLASNET_ROOT}/runtime/cartograph
            ${ATLASNET_ROOT}/.stage/cartograph
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:Web_swig>
            ${ATLASNET_ROOT}/.stage
    DEPENDS watchdog shard proxy Web_swig
    COMMENT "Staging AtlasNet binaries"
)
add_custom_target(AtlasnetDockerBuild_Fast
    COMMAND ${ATLASNET_ROOT}/docker/BuildDockerImages.sh
            -f ${ATLASNET_ROOT}/docker/dockerfiles/docker-bake.copy.json
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${ATLASNET_ROOT}/.stage
    WORKING_DIRECTORY ${ATLASNET_ROOT}
    USES_TERMINAL
)
add_dependencies(AtlasnetDockerBuild_Fast AtlasnetDockerBuild_Fast_Stage)
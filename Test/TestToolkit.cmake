# Factory function for sandbox targets
function(DEFINE_CUSTOM_TARGET target_name shard_image_name)
    add_custom_target(${target_name}
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev
                ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetDevStack.sh
                atlasnet_dev
                ${shard_image_name}
        USES_TERMINAL
        DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast
    )
endfunction()

function(DEFINE_CUSTOM_VALGRIND_TARGET target_name shard_image_name)
    add_custom_target(${target_name}_valgrind
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev
                ${CMAKE_SOURCE_DIR}/Dev/RunAtlasNetDevStack.sh
                atlasnet_dev
                ${shard_image_name}
                valgrind
        USES_TERMINAL
        DEPENDS SandboxServerDockerBuild AtlasnetDockerBuild_Fast
    )
endfunction()
function(DEFINE_CUSTOM_PERF_TARGET target_name target_dependencies command_line)
    add_custom_target(${target_name}
        ALL
        DEPENDS ${target_dependencies}
        COMMAND ${command_line}
        COMMENT "Running custom target ${target_name}"
    )
endfunction()

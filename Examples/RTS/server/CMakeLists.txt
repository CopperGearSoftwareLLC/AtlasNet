cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(RTSServer ${_sources})
target_link_libraries(RTSServer PUBLIC RTSLib_Server AtlasNetServer_Static)
# Optional, but common: local includes
target_include_directories(RTSServer
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

install(TARGETS RTSServer
    RUNTIME DESTINATION bin      # Linux/macOS executables
    COMPONENT RTSServer
)


set(RTS_SERVER_IMAGE_NAME "rts-server:latest" CACHE PATH "Image name of RTS server")
add_custom_target(RTSServerStage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RTS_ROOT}/.stage
    #COMMAND ${CMAKE_COMMAND} -E copy
    #        $<TARGET_FILE:AtlasNetServer_Shared>
    #        ${RTS_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:RTSServer>
            ${RTS_ROOT}/.stage/
    DEPENDS AtlasNetServer_Static
    COMMENT "Staging AtlasNetServer"
)
add_custom_target(RTSServerDockerBuild
    COMMAND ${CMAKE_COMMAND} -E echo "Starting Docker build..."
    COMMAND docker build
        --network=host
        --progress=plain
        -f ${RTS_ROOT}/RTSServer.dockerfile
        -t ${RTS_SERVER_IMAGE_NAME}
        ${RTS_ROOT}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${RTS_ROOT}/.stage
    WORKING_DIRECTORY ${RTS_ROOT}
    USES_TERMINAL
    COMMENT "Building RTSServer Docker image with live logs"
)
add_dependencies(RTSServerDockerBuild RTSServerStage)

add_custom_target(RTSServerDockerBuild_Fast
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/Dev
        ${CMAKE_SOURCE_DIR}/Dev/BuildRTSServerImageIfChanged.sh
        ${RTS_ROOT}
        ${RTS_SERVER_IMAGE_NAME}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${RTS_ROOT}/.stage
    COMMENT "Staging AtlasNetServer (changed-only image rebuild)"
)
add_dependencies(RTSServerDockerBuild_Fast RTSServerStage)

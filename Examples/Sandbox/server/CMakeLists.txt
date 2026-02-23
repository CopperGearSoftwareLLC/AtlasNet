cmake_minimum_required(VERSION 3.16)


# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(SandboxServer ${_sources})
target_link_libraries(SandboxServer PUBLIC SandboxLib AtlasNetServer_Static)
# Optional, but common: local includes
target_include_directories(SandboxServer
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

install(TARGETS SandboxServer
    RUNTIME DESTINATION bin      # Linux/macOS executables
    COMPONENT SandboxServer
)


set(SANDBOX_SERVER_IMAGE_NAME "sandbox-server:latest" CACHE PATH "Image name of sandbox server")
add_custom_target(SandboxServerStage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SANDBOX_ROOT}/.stage
    #COMMAND ${CMAKE_COMMAND} -E copy
    #        $<TARGET_FILE:AtlasNetServer_Shared>
    #        ${SANDBOX_ROOT}/.stage/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:SandboxServer>
            ${SANDBOX_ROOT}/.stage/
    DEPENDS AtlasNetServer_Static
    COMMENT "Staging AtlasNetServer"
)
add_custom_target(SandboxServerDockerBuild
    COMMAND ${CMAKE_COMMAND} -E chdir ${SANDBOX_ROOT}
        docker build
        -f SandboxServer.dockerfile
        -t ${SANDBOX_SERVER_IMAGE_NAME}
        .
    COMMAND ${CMAKE_COMMAND} -E rm -rf {SANDBOX_ROOT}/.stage
    COMMENT "Staging AtlasNetServer"
)
add_dependencies(SandboxServerDockerBuild SandboxServerStage)
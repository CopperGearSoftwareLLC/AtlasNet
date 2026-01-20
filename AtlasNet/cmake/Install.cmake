# Set default installation directories
include(GNUInstallDirs)

# Use the user-specified prefix, or default to ./package
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/package" CACHE PATH "Install path prefix" FORCE)
endif()

# Set all AtlasNet install directories relative to CMAKE_INSTALL_PREFIX
set(ATLASNET_INSTALL_ROOT ${CMAKE_INSTALL_PREFIX})
set(ATLASNET_INSTALL_BIN     "${ATLASNET_INSTALL_ROOT}/bin")
set(ATLASNET_INSTALL_DEPS    "${ATLASNET_INSTALL_ROOT}/deps")
set(ATLASNET_INSTALL_SDK     "${ATLASNET_INSTALL_ROOT}/sdk")
set(ATLASNET_INSTALL_RUNTIME_APPS  "${ATLASNET_INSTALL_ROOT}/runtime")
set(ATLASNET_INSTALL_SCHEMA  "${ATLASNET_INSTALL_ROOT}/schemas")
set(ATLASNET_INSTALL_LIB     "${ATLASNET_INSTALL_ROOT}/lib")
set(ATLASNET_INSTALL_CMAKE   "${ATLASNET_INSTALL_ROOT}/cmake")


SET(ATLASNET_INSTALL_BOOTSTRAP_CMD AtlasNetBootstrap)
SET(ATLASNET_INSTALL_INDOCKER_CMD AtlasNetDocker)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/
    DESTINATION ${ATLASNET_INSTALL_SDK}   # relative to CMAKE_INSTALL_PREFIX
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cpp"
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.txt"
)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/apps/
    DESTINATION ${ATLASNET_INSTALL_RUNTIME_APPS}   # relative to CMAKE_INSTALL_PREFIX
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cpp"
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.hpp"
        PATTERN "*.txt"
        PATTERN "*.js"
        PATTERN "*.ts"
        PATTERN "*.tsx"
        PATTERN "*.jsx"
        PATTERN "*.svg"
        PATTERN "*.json"
        PATTERN "*.mjs"
        PATTERN "*.cjs"
        PATTERN "*.css"

)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt 
    DESTINATION ${ATLASNET_INSTALL_CMAKE}
    COMPONENT 
    ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AtlasNetSDK.cmake
    DESTINATION ${ATLASNET_INSTALL_ROOT}
    COMPONENT 
    ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)
install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/AtlasNetSetup.sh
    DESTINATION ${ATLASNET_INSTALL_CMAKE}
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
)
# Copy the entire ./cmake folder
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cmake/
    DESTINATION ${ATLASNET_INSTALL_CMAKE}/cmake
    COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
    FILES_MATCHING
        PATTERN "*.cmake"
        PATTERN "CMakeLists.txt"  # if you have any inside cmake/
)
# Conditionally install deps folder if it exists
#if(EXISTS "${CMAKE_SOURCE_DIR}/deps")
#    message(STATUS "Found deps folder, adding install directive.")
#    install(
#        DIRECTORY ${CMAKE_SOURCE_DIR}/deps/
#        DESTINATION ${ATLASNET_INSTALL_DEPS}
#        COMPONENT ${ATLASNET_INSTALL_BOOTSTRAP_CMD}
#        FILES_MATCHING
#        PATTERN "*.h"
#        PATTERN "*.hpp"
#        PATTERN "*.c"
#        PATTERN "*.cpp"
#        PATTERN "*.ipp"
#        #PATTERN "*.a"
#        # Optional: you can filter by file type if needed, or remove FILES_MATCHING to copy everything
#    )
#endif()
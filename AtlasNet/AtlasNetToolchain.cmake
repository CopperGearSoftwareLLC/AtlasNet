# AtlasNetToolchain.cmake
# Purpose:
#  - Wire vcpkg
#  - Provide AtlasNet root hints
#  - NO targets
#  - NO add_subdirectory
#  - NO compile definitions

# ------------------------------
# Compilers (optional override)
# ------------------------------
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/gcc CACHE FILEPATH "")
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/g++ CACHE FILEPATH "")
endif()

# ------------------------------
# vcpkg integration
# ------------------------------
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR
        "AtlasNet toolchain requires VCPKG_ROOT to be set.\n"
        "Please install vcpkg and export VCPKG_ROOT."
    )
endif()

set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE FILEPATH "vcpkg toolchain"
)

# Use build-local vcpkg tree
set(VCPKG_INSTALLED_DIR
    "${CMAKE_BINARY_DIR}/vcpkg_installed"
    CACHE PATH "vcpkg installed dir"
)

message(STATUS "AtlasNet toolchain: vcpkg enabled")

# ------------------------------
# AtlasNet hint paths (IMPORTANT)
# ------------------------------
# Lets consumers do:
#   find_package(AtlasNet CONFIG REQUIRED)

set(ATLASNET_ROOT
    "${CMAKE_CURRENT_LIST_DIR}"
    CACHE PATH "AtlasNet root directory"
)

list(APPEND CMAKE_PREFIX_PATH
    "${ATLASNET_ROOT}/package"
)

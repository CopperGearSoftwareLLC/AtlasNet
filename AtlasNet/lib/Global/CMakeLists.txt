cmake_minimum_required(VERSION 3.16)

set(TARGET_NAME AtlasNetGlobal)

# Collect sources
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${TARGET_NAME} STATIC ${_sources})

# Find Boost first
set(BOOST_ROOT "${ATLASNET_ROOT}/deps/boost_install")  # path to your local Boost
set(BOOST_INCLUDEDIR "${BOOST_ROOT}/include")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")

message(STATUS ${BOOST_INCLUDEDIR})
# Optional: Node.js includes (if needed)
# target_include_directories(${TARGET_NAME} PUBLIC ${NODE_INCLUDE_DIR} ${NODE_ADDON_API_INCLUDE})

# Include directories
target_include_directories(${TARGET_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        ${BOOST_INCLUDEDIR}   # now Boost headers are included
)
file(GLOB BOOST_LIBS "${BOOST_LIBRARYDIR}/*.a")
# Link libraries
target_link_libraries(${TARGET_NAME}
    PUBLIC
        Boost::bimap
        Boost::describe
        Boost::dynamic_bitset
        Boost::flyweight
        Boost::math
        Boost::multi_array
        Boost::multi_index
        Boost::stacktrace_addr2line
        Boost::static_string
        Boost::uuid
)

# Other dependencies

target_link_libraries(${TARGET_NAME}
    PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
)

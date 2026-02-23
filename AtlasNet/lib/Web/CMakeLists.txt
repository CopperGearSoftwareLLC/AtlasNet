cmake_minimum_required(VERSION 3.16)

#set(TARGET_NAME MyCustomLibName)

# Use user-defined name if provided, otherwise use folder name
if(NOT DEFINED TARGET_NAME)
    get_filename_component(TARGET_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
endif()

# Recursively collect sources under ./src
file(GLOB_RECURSE _sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(${TARGET_NAME} STATIC ${_sources})

# Optional, but common: local includes
target_include_directories(${TARGET_NAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    
)


target_link_libraries(${TARGET_NAME}
    Native
)

add_swig_library_for_target(${TARGET_NAME}
    STL_INCLUDES
        "std_vector.i"
        "std_map.i"
        "std_pair.i"
        "std_string.i"
    STL_TEMPLATES
        "std::vector<IBoundsDrawShape>"
        "std::vector<std::pair<float,float>>"
        "std::pair<float,float>"
        "std::vector<std::string>"
        "std::vector<std::vector<std::string>>"
        "std::vector<EntityLedgerEntry>"

    #   "std::unordered_map<IBounds::BoundsID, ByteReader>"
)


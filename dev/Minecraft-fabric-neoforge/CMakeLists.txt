cmake_minimum_required(VERSION 3.16)

# Target name = this folder name (nice for add_subdirectory reuse)
get_filename_component(_target_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)




add_custom_target(${_target_name} ALL
  COMMAND ./gradlew build
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/mc
  BYPRODUCTS
    ${CMAKE_SOURCE_DIR}/mc/fabric/build/libs
    ${CMAKE_SOURCE_DIR}/mc/neoforge/build/libs
)

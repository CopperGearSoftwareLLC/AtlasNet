cmake_minimum_required(VERSION 3.10)
include(CTest)
include(TestDeps.cmake)
include(GoogleTest)

file(GLOB_RECURSE _Test_Utils_Sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Utils/*.cpp"
)

add_library(AtlasNetTestUtils STATIC ${_Test_Utils_Sources})
target_include_directories(AtlasNetTestUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
target_link_libraries(AtlasNetTestUtils PUBLIC AtlasNetCore raylib)

add_subdirectory(Unit)
add_subdirectory(Integration)


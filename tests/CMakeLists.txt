cmake_minimum_required(VERSION 3.10)
include(CTest)
include(TestDeps.cmake)
include(GoogleTest)

add_library(AtlasNetTestUtils STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Utils/TestUtils.cpp)
target_include_directories(AtlasNetTestUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
target_link_libraries(AtlasNetTestUtils PUBLIC AtlasNetCore)

add_subdirectory(Unit)
add_subdirectory(Integration)


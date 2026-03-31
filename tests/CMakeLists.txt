cmake_minimum_required(VERSION 3.10)
include(CTest)
include(TestDeps.cmake)
include(GoogleTest)

add_subdirectory(Unit)
add_subdirectory(Integration)
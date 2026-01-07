cmake_minimum_required(VERSION 3.24)
set(CMAKE_TOOLCHAIN_FILE "AtlasNet/AtlasNetToolchain.cmake" CACHE FILEPATH "AtlasNet toolchain")
project(SampleSetup)



add_subdirectory(AtlasNet)


add_subdirectory(Examples/Sandbox)
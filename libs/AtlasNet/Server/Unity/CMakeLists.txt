cmake_minimum_required(VERSION 3.16)
project(KDNetUnityAPI LANGUAGES CXX)

add_library(KDNetUnityAPI SHARED KDNetUnityAPI.cpp)
target_compile_features(KDNetUnityAPI PUBLIC cxx_std_17)

if(WIN32)
  # Make sure the file is exactly KDNetUnityAPI.dll
  set_target_properties(KDNetUnityAPI PROPERTIES PREFIX "" OUTPUT_NAME "KDNetUnityAPI")

  # Link WinHTTP (ships with Windows)
  target_link_libraries(KDNetUnityAPI PRIVATE winhttp)

  # --- ADD THESE TWO LINES FOR MINGW STATIC RUNTIMES ---
  if(MINGW)
    target_link_options(KDNetUnityAPI PRIVATE -static-libgcc -static-libstdc++)
  endif()

else()
  find_package(CURL REQUIRED)
  target_link_libraries(KDNetUnityAPI PRIVATE CURL::libcurl)
endif()

cmake_minimum_required(VERSION 3.16)
project(KDNetUnityAPI LANGUAGES CXX)

add_library(KDNetUnityAPI SHARED KDNetUnityAPI.cpp)
target_compile_features(KDNetUnityAPI PUBLIC cxx_std_17)

# On Unix-like, PIC is standard; ensure it's on
set_target_properties(KDNetUnityAPI PROPERTIES POSITION_INDEPENDENT_CODE ON)

# When targeting Windows (MSVC or MinGW), drop the 'lib' prefix so Unity finds KDNetUnityAPI.dll
if(WIN32)
  set_target_properties(KDNetUnityAPI PROPERTIES PREFIX "")
endif()

# (Optional) If you cross-compile with MinGW and want to avoid extra runtime DLLs:
# target_link_options(KDNetUnityAPI PRIVATE -static-libgcc -static-libstdc++)

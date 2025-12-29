// AtlasNetAPI.hpp
#pragma once

// When building any AtlasNet DLL, define ATLASNET_BUILD in that target.
// When consuming AtlasNet, do NOT define ATLASNET_BUILD.

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef ATLASNET_BUILD
    #define ATLASNET_API __declspec(dllexport)
  #else
    #define ATLASNET_API __declspec(dllimport)
  #endif
  #define ATLASNET_LOCAL
#else
  #if __GNUC__ >= 4
    #define ATLASNET_API   __attribute__((visibility("default")))
    #define ATLASNET_LOCAL __attribute__((visibility("hidden")))
  #else
    #define ATLASNET_API
    #define ATLASNET_LOCAL
  #endif
#endif

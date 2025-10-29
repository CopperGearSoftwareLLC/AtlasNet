/**
 * @file AtlasUnityBridge.h
 * @brief Unity plugin wrapped from AtlasNetServer + additional features
 * 
 */

#pragma once
#include <stdint.h>
#include "../../AtlasNet/AtlasEntity.hpp"
#include "../../AtlasNet/Server/AtlasNetServer.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro
#if defined(_WIN32)
  #define ATLAS_API __declspec(dllexport)
#else
  #define ATLAS_API __attribute__((visibility("default")))
#endif

// Simple struct to initialize the server
typedef struct AtlasInitializeProperties {
    const char* exe_path;   // optional, for CrashHandler
} AtlasInitializeProperties;

// Initialize AtlasNetServer
ATLAS_API int atlas_initialize(const AtlasInitializeProperties* props);

// Tick update: accepts entities from Unity, outputs incoming/outgoing arrays
ATLAS_API int atlas_update(
    AtlasEntity* entities_in,
    int32_t entities_in_count,
    AtlasEntity* entities_out,
    int32_t* entities_out_count,
    uint64_t* outgoing_ids,
    int32_t* outgoing_ids_count
);

// Clean shutdown
ATLAS_API void atlas_shutdown(void);

#ifdef __cplusplus
}
#endif

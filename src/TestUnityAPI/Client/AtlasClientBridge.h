#pragma once
#include <stdint.h>
#include "../../AtlasNet/AtlasEntity.hpp"
#include "../../AtlasNet/Client/AtlasNetClient.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define ATLAS_API __declspec(dllexport)
#else
  #define ATLAS_API __attribute__((visibility("default")))
#endif

typedef struct AtlasClientInitProperties {
    const char* exe_path;             // e.g. "/atlasnet/Client" or Application.dataPath
    const char* client_name;          // optional; if null -> DockerIO self name
    const char* target_server_name;   // optional; if null -> same as client_name
} AtlasClientInitProperties;

// returns 0 on success
ATLAS_API int  atlas_client_initialize(const AtlasClientInitProperties* props);
ATLAS_API void atlas_client_update(void);
ATLAS_API void atlas_client_send_input(AtlasEntity* player_intent); // pass a minimal intent (ID/Position)
ATLAS_API void atlas_client_shutdown(void);

#ifdef __cplusplus
}
#endif

#pragma once
#include <stdint.h>
#include "../../AtlasNet/AtlasEntity.hpp"
#include "../../AtlasNet/Client/AtlasNetClient.hpp"

#ifdef _WIN32
#define ATLAS_API __declspec(dllexport)
#else
#define ATLAS_API __attribute__((visibility("default")))
#endif

extern "C" {
// returns 0 on success
ATLAS_API int atlas_client_initialize(const char *exe_path, const char *client_name, const char *server_name);
ATLAS_API void atlas_client_send_entity(AtlasEntity entity);
ATLAS_API int atlas_client_get_entities(AtlasEntity *buffer, int maxCount);
ATLAS_API void atlas_client_shutdown(void);
}
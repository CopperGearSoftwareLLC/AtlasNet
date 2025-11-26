#include "AtlasClientBridge.h"
#include <memory>
#include <cstdio>

static std::unique_ptr<AtlasNetClient> g_client;

extern "C" int atlas_client_initialize(const char *exe_path, const char *client_name, const char *server_name)
{
    g_client = std::make_unique<AtlasNetClient>();
    AtlasNetClient::InitializeProperties props{
        .ExePath = exe_path,
        .ServerName = server_name
    };

    try {
        g_client->Initialize(props);
    } catch (const std::exception &e) {
        fprintf(stderr, "[AtlasClientBridge] Exception during init: %s\n", e.what());
        return -2;
    }
    return 0;
}

extern "C" void atlas_client_send_entity(AtlasEntity entity)
{
    if (g_client) g_client->SendEntityUpdate(entity);
}

extern "C" int atlas_client_get_entities(AtlasEntity *buffer, int maxCount)
{
  return 0;
    if (!g_client) return 0;
    return g_client->GetRemoteEntities(buffer, maxCount);
}
extern "C" void atlas_client_shutdown(void)
{
    if (g_client)
    {
        g_client->Shutdown();
        g_client.reset();
    }
}

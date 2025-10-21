#include "AtlasClientBridge.h"
#include <memory>
#include <cstdio>

static std::unique_ptr<AtlasNetClient> g_client;

extern "C" int atlas_client_initialize(const AtlasClientInitProperties* props)
{
    try
    {
        AtlasNetClient::InitializeProperties init{};
        init.ExePath = (props && props->exe_path) ? props->exe_path : ".";
        //if (props && props->client_name)        init.ClientNameOverride       = props->client_name;
        //if (props && props->target_server_name) init.TargetServerNameOverride = props->target_server_name;

        g_client = std::make_unique<AtlasNetClient>();
        g_client->Initialize(init);
        return 0;
    }
    catch (...)
    {
        return -1;
    }
}

extern "C" void atlas_client_update(void)
{
    if (g_client) g_client->Update();
}

extern "C" void atlas_client_send_input(AtlasEntity* player_intent)
{
    if (g_client && player_intent) g_client->SendInputIntent(*player_intent);
}

extern "C" void atlas_client_shutdown(void)
{
    if (g_client)
    {
        g_client->Shutdown();
        g_client.reset();
    }
}

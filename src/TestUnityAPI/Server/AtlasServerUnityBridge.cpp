#include "AtlasServerUnityBridge.h"
#include <vector>
#include <memory>
#include <cstring>

static std::unique_ptr<AtlasNetServer> g_server;

static void RedirectStdToUnityLog()
{
    // Unity’s Player.log path at runtime
    const char* logPath = "/atlasnet/logs/gameserver.log";
    FILE* logFile = fopen(logPath, "a");
    if (!logFile)
        return;

    int fd = fileno(logFile);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

// --------- Initialize ----------
extern "C" int atlas_initialize(const AtlasInitializeProperties* props)
{
    RedirectStdToUnityLog();   // <-- add this line
    printf("[AtlasUnityBridge] Redirected stdout/stderr to Unity log\n");
    fflush(stdout);


    try {
        g_server = std::make_unique<AtlasNetServer>();

        AtlasNetServer::InitializeProperties initProps;

        g_server->Initialize(initProps);
        return 0;
    }
    catch (...) {
        return -1;
    }
}

// --------- Update ----------
extern "C" int atlas_update(
    AtlasEntity* entities_in,
    int32_t entities_in_count,
    AtlasEntity* entities_out,
    int32_t* entities_out_count,
    uint64_t* outgoing_ids,
    int32_t* outgoing_ids_count)
{
    if (!g_server) return -1;

    std::span<AtlasEntity> inSpan(entities_in, entities_in_count);
    std::vector<AtlasEntity> incoming;
    std::vector<AtlasEntityID> outgoing;

    g_server->Update(inSpan, incoming, outgoing);

    // For now, the backend isn’t sending anything; just clear counts.
    *entities_out_count = static_cast<int32_t>(incoming.size());
    *outgoing_ids_count = static_cast<int32_t>(outgoing.size());
    if (*entities_out_count > 0 && entities_out)
        std::memcpy(entities_out, incoming.data(), sizeof(AtlasEntity) * *entities_out_count);
    if (*outgoing_ids_count > 0 && outgoing_ids)
        std::memcpy(outgoing_ids, outgoing.data(), sizeof(AtlasEntityID) * *outgoing_ids_count);

    return 0;
}

// --------- Shutdown ----------
extern "C" void atlas_shutdown(void)
{
    g_server.reset();
}

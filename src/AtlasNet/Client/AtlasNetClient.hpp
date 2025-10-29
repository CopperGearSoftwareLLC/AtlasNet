#pragma once
#include "pch.hpp"
#include <memory>
#include <mutex>
#include "AtlasNet/AtlasNetInterface.hpp"
#include "Singleton.hpp"
#include "../AtlasEntity.hpp"
#include "../../Interlink/Interlink.hpp"
#include "../../Debug/Crash/CrashHandler.hpp"
#include "../../Docker/DockerIO.hpp"

class AtlasNetClient: public AtlasNetInterface, public Singleton<AtlasNetClient>
{
public:
    struct InitializeProperties
    {
        std::string ExePath;
        std::string ClientName;
        std::string ServerName;
    };

public:
    AtlasNetClient() = default;
    void Initialize(InitializeProperties& props);
    void SendEntityUpdate(const AtlasEntity &entity);
    int GetRemoteEntities(AtlasEntity *buffer, int maxCount);
    void Shutdown();
private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetClient");;
    std::unordered_map<AtlasEntityID, AtlasEntity> RemoteEntities;
    std::mutex Mutex;
    InterLinkIdentifier serverID;
};

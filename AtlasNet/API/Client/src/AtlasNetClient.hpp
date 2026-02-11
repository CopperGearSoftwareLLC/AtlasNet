#pragma once
#include "pch.hpp"
#include <memory>
#include <mutex>

#include "Misc/Singleton.hpp"
#include "Entity.hpp"
#include "Entity.hpp"

#include "Crash/CrashHandler.hpp"
#include "DockerIO.hpp"
#include "AtlasNet.hpp"
#include "Network/IPAddress.hpp"
#include "Log.hpp"
class AtlasNetClient: public Singleton<AtlasNetClient>
{
public:
    struct InitializeProperties
    {
        IPAddress AtlasNetProxyIP;
    };

public:
    AtlasNetClient() = default;
    void Initialize(InitializeProperties& props);
    void Tick();
//    void SendEntityUpdate(const AtlasEntity &entity);
//    int GetRemoteEntities(AtlasEntity *buffer, int maxCount);
    void Shutdown();
private:
//    void OnConnected(const InterLinkIdentifier &identifier);
//    void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
//    void HandleEntityMessage(const std::span<const std::byte>& data);
private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetClient");
    std::unordered_map<AtlasEntity::EntityID, AtlasEntity> RemoteEntities;
    std::mutex Mutex;
    bool IsConnectedToProxy = false;
};

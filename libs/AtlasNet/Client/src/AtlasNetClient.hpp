#pragma once
#include "pch.hpp"
#include <memory>
#include <mutex>
#include "AtlasNet/AtlasNetInterface.hpp"
#include "misc/Singleton.hpp"
#include "AtlasNet/AtlasEntity.hpp"
#include "Interlink/Interlink.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerIO.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "Interlink/Connection.hpp"
class AtlasNetClient: public AtlasNetInterface, public Singleton<AtlasNetClient>
{
public:
    struct InitializeProperties
    {
        IPAddress GameCoordinatorAddress;
    };

public:
    AtlasNetClient() = default;
    void Initialize(InitializeProperties& props);
    void Tick();
    void SendEntityUpdate(const AtlasEntity &entity);
    int GetRemoteEntities(AtlasEntity *buffer, int maxCount);
    void Shutdown();
private:
    void OnConnected(const InterLinkIdentifier &identifier);
    void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
    void HandleEntityMessage(const std::span<const std::byte>& data);
private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetClient");
    std::unordered_map<AtlasEntityID, AtlasEntity> RemoteEntities;
    std::mutex Mutex;
    InterLinkIdentifier myID;
    InterLinkIdentifier proxyID;
    InterLinkIdentifier GameCoordinatorID;
    bool IsConnectedToProxy = false;
};

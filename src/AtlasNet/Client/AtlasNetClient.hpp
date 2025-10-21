#pragma once
#include "pch.hpp"
#include <memory>
#include <vector>
#include <span>
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
        std::string TargetServerName;
        std::string TargetServerIP;     // direct IP for now
    };

public:
    AtlasNetClient() = default;
    void Initialize(InitializeProperties& props);
    void Update();
    void SendInputIntent(const AtlasEntity& playerIntent);
    void Shutdown();

private:
    std::shared_ptr<Log> logger;
    InterLinkIdentifier serverID;
    bool connected = false;
};

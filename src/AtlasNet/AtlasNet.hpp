#pragma once
//#include "Server/AtlasNetServer.hpp"
//#include "Client/AtlasNetClient.hpp"
#include "Singleton.hpp"
#include "pch.hpp"
#include "Debug/Log.hpp"
enum class AtlasNetMessageHeader : uint8_t
{   
    Null = 255,
    EntityUpdate = 0,
    EntityIncoming = 1,
    EntityOutgoing = 2,
    FetchGridShape = 3
};
struct Worker
{
    std::string Name;
    std::string IP;
};
struct AtlasNetSettings
{
    std::vector<Worker> workers;

    std::unordered_set<std::string> SourceDirectories;
    std::vector<std::string> GameServerBuildCmds,GameServerRunCmds;
    std::unordered_set<std::string> RuntimeArches;
    std::string BuildCacheDir;
    std::string NetworkInterface;
    std::optional<uint32> BuilderMemoryGb;
};
class AtlasNet : public Singleton<AtlasNet>
{
    Log log = Log("AtlasNet");
    const static inline std::string AtlasNetSettingsFile = "AtlasNetSettings.json";
    const AtlasNetSettings settings;
    
    static AtlasNetSettings ParseSettingsFile();
    public:
    const auto& GetSettings() const {return settings;}
    AtlasNet();
};

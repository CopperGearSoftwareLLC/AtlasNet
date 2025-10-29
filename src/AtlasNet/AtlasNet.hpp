#pragma once
#include "Server/AtlasNetServer.hpp"
#include "Client/AtlasNetClient.hpp"
#include "Singleton.hpp"
struct Worker
{
    std::string Name;
    std::string IP;
};
struct AtlasNetSettings
{
    std::vector<Worker> workers;
    std::string GameServerBinary;
    std::string GameServerFiles;
    std::unordered_set<std::string> RuntimeArches;
    std::string BuildCacheDir;
    std::string NetworkInterface;
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
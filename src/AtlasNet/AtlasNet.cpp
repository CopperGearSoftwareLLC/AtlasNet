#include "AtlasNet.hpp"

AtlasNetSettings AtlasNet::ParseSettingsFile()
{
    AtlasNetSettings settings;
    std::ifstream settingsFile(AtlasNetSettingsFile);
    if (!settingsFile.is_open())
    {
        // log.ErrorFormatted("Unable to find {} file",AtlasNetSettingsFile);
        throw "Unable to find AtlasNetSettings.json file";
    }
    Json parsedJson;
    settingsFile >> parsedJson;
    settingsFile.close();

    if (parsedJson.contains("Workers"))
    {
        for (const auto &workerJ : parsedJson["Workers"])
        {
            Worker worker;
            worker.IP = workerJ["Address"];
            worker.Name = workerJ["Name"];
            settings.workers.push_back(worker);
        }
    }

    settings.GameServerFiles = parsedJson.contains("GameServerFiles") ? parsedJson["GameServerFiles"] : "";
    settings.GameServerBinary = parsedJson.contains("GameServerBinary") ? parsedJson["GameServerBinary"] : "";
    settings.GameServerBinary = parsedJson["GameServerBinary"];
    settings.RuntimeArches;
    for (const auto &arch : parsedJson["RuntimeArches"])
    {
        settings.RuntimeArches.insert(arch);
    }
    settings.BuildCacheDir = parsedJson.contains("BuildCacheDir") ? parsedJson["BuildCacheDir"] : "";
    settings.NetworkInterface = parsedJson.contains("NetworkInterface") ? parsedJson["NetworkInterface"] : "";
    if (parsedJson.contains("BuilderMemoryGb"))
    {
        settings.BuilderMemoryGb = parsedJson["BuilderMemoryGb"].get<uint32>();
    }
    return settings;
}

AtlasNet::AtlasNet() : settings(ParseSettingsFile())
{
}
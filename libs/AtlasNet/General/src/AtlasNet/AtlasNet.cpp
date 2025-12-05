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
    Ordered_Json parsedJson;
    settingsFile >> parsedJson;
    settingsFile.close();
    auto IsEntryValid = [](const Json &json, const std::string &str)
    {
        return json.contains(str) && !json[str].is_null();
    };
    if (IsEntryValid(parsedJson, "Workers"))
    {
        for (const auto &workerJ : parsedJson["Workers"])
        {
            Worker worker;
            worker.IP = workerJ["Address"];
            worker.Name = workerJ["Name"];
            settings.workers.push_back(worker);
        }
    }
    if (IsEntryValid(parsedJson, "GameServerDockerFile"))
    {
        settings.GameServerDockerFile = parsedJson["GameServerDockerFile"].get<std::string>();
    }
    if (IsEntryValid(parsedJson, "GameServerRunCommand"))
    {
        settings.GameServerRunCommand = parsedJson["GameServerRunCommand"].get<std::string>();
    }

    // --- GameServerRunCmd ---
    settings.RuntimeArches;
    for (const auto &arch : parsedJson["RuntimeArches"])
    {
        settings.RuntimeArches.insert(arch);
    }
    settings.BuildCacheDir = IsEntryValid(parsedJson, "BuildCacheDir") ? parsedJson["BuildCacheDir"] : "";
    settings.NetworkInterface = IsEntryValid(parsedJson, "NetworkInterface") ? parsedJson["NetworkInterface"] : "";
    if (IsEntryValid(parsedJson, "BuilderMemoryGb"))
    {
        settings.BuilderMemoryGb = parsedJson["BuilderMemoryGb"].get<uint32>();
    }
    if (IsEntryValid(parsedJson, "TLSDirectory"))
    {
        settings.TlsDir = parsedJson["TLSDirectory"].get<std::string>();
    }
    return settings;
}

AtlasNet::AtlasNet() : settings(ParseSettingsFile())
{
}
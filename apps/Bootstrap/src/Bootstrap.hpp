#pragma once

#include "Debug/Log.hpp"
class Bootstrap 
{
    bool MultiNodeMode = false;
    std::string ManagerAdvertiseAddress;
    Log logger = Log("");
    void InitSwarm();
    void CreateTLS();
    void SendTLSToWorkers();
    void JoinWorkers();
    void SetupBuilder();
    void SetupNetwork();
    void SetupRegistry();
    void BuildImages();
    void BuildGameServer();
    void DeployStack();

    void BuildDockerImageLocally(const std::string& DockerFileContent,const std::string& ImageName);
    void BuildDockerImageBuildX(const std::string& DockerFileContent,const std::string& ImageName,const std::unordered_set<std::string>& arches);

    public:
    void Run();
};
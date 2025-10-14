#pragma once
#include <pch.hpp>
#include <Singleton.hpp>

using DockerContainerID = std::string;
class DockerIO : public Singleton<DockerIO>
{
    std::string unixSocket;
    mutable std::optional<nlohmann::json> SelfJson;
public:
    DockerIO(const std::string &sockPath = "/var/run/docker.sock") : unixSocket(sockPath)
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~DockerIO() {
        curl_global_cleanup();
    }
    std::string request(const std::string& method,
                        const std::string& endpoint,
                        const nlohmann::json* body = nullptr,
                        const std::vector<std::string>& extraHeaders = {})const ;

    std::string InspectContainer(const DockerContainerID& ContainerID)const ; 

    nlohmann::json InspectSelf() const;
    std::string GetSelfContainerName() const;
    std::string GetSelfContainerIP() const;
    /// @brief Get the exposed port of this container
    /// @return returns a list of port pairs. Internal/External
    std::vector<std::pair<uint32,uint32>> GetSelfExposedPorts() const;
    std::optional<uint32> GetSelfExposedPortForInternalBind(uint32 InternalPort) const;
    
};
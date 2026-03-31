#pragma once
#include <Global/pch.hpp>
#include <Global/Misc/Singleton.hpp>
#include <curl/curl.h>
class Curl:public Singleton<Curl>
{
    public:
    Curl()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);

    }
    ~Curl()
    {
        curl_global_cleanup();

    }
};
using DockerContainerID = std::string;
class DockerIO : public Singleton<DockerIO>
{
    std::string unixSocket;
    mutable std::optional<nlohmann::json> SelfJson;
public:
    DockerIO(const std::string &sockPath = "/var/run/docker.sock") : unixSocket(sockPath)
    {
        Curl::Get();
    }
    ~DockerIO() {
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
    std::vector<std::pair<uint32_t,uint32_t>> GetSelfExposedPorts() const;
    std::optional<uint32_t> GetSelfExposedPortForInternalBind(uint32_t InternalPort) const;
    std::string GetSelfPublicIP(uint32_t& outPort) const;
std::optional<uint32_t> GetSelfPublicPortFor(uint32_t internalPort) const;

    // grab published port for service
    std::optional<uint32_t> GetServicePublishedPort(const std::string& serviceName,
    uint32_t internalPort,
    const std::string& protocol = "udp") const;
    // grab published IP for service
    std::optional<std::string> GetServiceNodePublicIP(const std::string& serviceName) const;
    
private:
    // URL-escaped JSON filters for Swarm API
    std::string EncodeFilters(const nlohmann::json& filters) const;

};
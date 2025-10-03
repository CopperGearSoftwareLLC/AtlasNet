#pragma once
#include <pch.hpp>
#include <Singleton.hpp>

using DockerContainerID = std::string;
class DockerIO : public Singleton<DockerIO>
{
    std::string unixSocket;
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
                        const std::vector<std::string>& extraHeaders = {});

    std::string InspectContainer(const DockerContainerID& ContainerID);
};
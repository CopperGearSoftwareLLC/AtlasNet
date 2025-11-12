#pragma once
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Debug/Log.hpp"
#include "Database/ProxyRegistry.hpp"
#include "Docker/DockerIO.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>
#include <thread>

/**
 * @brief The Demigod acts as a proxy between external game clients
 *        and internal game servers or partitions. It uses Interlink
 *        to route messages bidirectionally.
 */
class Demigod
{
public:
    Demigod();
    ~Demigod();

    void Init();
    void Run();
    void Shutdown();
private:
    void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
    void OnConnected(const InterLinkIdentifier& id);
    bool OnAcceptConnection(const Connection& c);

    void ForwardClientToServer(const Connection& from, std::span<const std::byte> data);
    void ForwardServerToClient(const Connection& from, std::span<const std::byte> data);

    std::shared_ptr<Log> logger = std::make_shared<Log>("Demigod");
    std::unordered_map<std::string, InterLinkIdentifier> clientToServerMap;
    bool ShouldShutdown = false;
};

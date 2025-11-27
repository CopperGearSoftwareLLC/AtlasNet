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

    static Demigod& Get()
    {
        static Demigod instance;
        return instance;
    }

private:
    // --- Interlink callbacks ------------------------------------------------

    bool OnAcceptConnection(const Connection& c);
    void OnConnected(const InterLinkIdentifier& id);
    void OnDisconnected(const InterLinkIdentifier& id);
    void OnMessageReceived(const Connection& from, std::span<const std::byte> data);

    // --- Routing helpers ----------------------------------------------------

    void ForwardClientToPartition(const Connection& from, std::span<const std::byte> data);
    void ForwardPartitionToClient(const Connection& from, std::span<const std::byte> data);

    // Handle text-based control messages like "AuthorityChange:..."
    void HandleControlMessage(const Connection& from, const std::string& msg);
private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("Demigod");
    bool ShouldShutdown = false;

        InterLinkIdentifier  SelfID;

    // Known partitions this proxy is connected to
    std::unordered_set<InterLinkIdentifier> partitions;

    std::unordered_set<InterLinkIdentifier> clients;
    // String-ified client IDs connected to this proxy
    std::unordered_set<std::string> clientStringIDs;

    // Routing: client identifier string -> authoritative partition ID
    std::unordered_map<std::string, InterLinkIdentifier> clientToPartitionMap;
};

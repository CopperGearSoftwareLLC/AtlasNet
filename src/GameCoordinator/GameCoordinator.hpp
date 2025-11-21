#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Interlink/Interlink.hpp"
#include "Heuristic/Heuristic.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ProxyRegistry.hpp"

class GameCoordinator : public Singleton<GameCoordinator>
{
public:
    GameCoordinator();
    ~GameCoordinator();

    void Init();
    void Run();
    void Shutdown();

private:
    bool OnAcceptConnection(const Connection& c);
    void OnConnected(const InterLinkIdentifier& id);
    void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
    private:
    void StartClientProxyHandshake(const InterLinkIdentifier& clientID);    
    std::unordered_map<std::string, InterLinkIdentifier> pendingClientAssignments;
    void AssignClientToDemigod(const InterLinkIdentifier& clientID);

private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("GameCoordinator");
    bool ShouldShutdown = false;
};
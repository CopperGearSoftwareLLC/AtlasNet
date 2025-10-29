#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Interlink/Interlink.hpp"
#include "Externlink/Externlink.hpp"
#include "Heuristic/Heuristic.hpp"
#include "Database/RedisCacheDatabase.hpp"

class GameCoordinator : public Singleton<GameCoordinator>
{
public:
    GameCoordinator();
    ~GameCoordinator();

    bool Init();
    void Run();
    void Shutdown();

private:
    std::shared_ptr<Log> logger = std::make_shared<Log>("GameCoordinator");
    Externlink Link;
    std::unordered_map<uint64_t, std::string> Clients;
    std::atomic<bool> ShouldShutdown = false;

    void OnClientConnected(const ExternlinkConnection& conn);
    void OnClientDisconnected(const ExternlinkConnection& conn);
    void OnClientMessage(const ExternlinkConnection& conn, std::string_view msg);
};
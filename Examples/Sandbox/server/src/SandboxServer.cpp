#include "SandboxServer.hpp"
#include "AtlasNetServer.hpp"
#include <thread>
#include <chrono>

void SandboxServer::Run()
{
    
    AtlasNetServer::InitializeProperties InitProperties;
    InitProperties.OnShutdownRequest = [&](SignalType signal)
    { ShouldShutdown = true; };
    AtlasNetServer::Get().Initialize(InitProperties);
//
    //using clock = std::chrono::high_resolution_clock;
    //auto previous = clock::now();

    while (!ShouldShutdown)
    {
    
        std::span<AtlasEntity> myspan;
        std::vector<AtlasEntity> Incoming;
        std::vector<AtlasEntityMinimal::EntityID> Outgoing;
        AtlasNetServer::Get().Update(myspan, Incoming, Outgoing);
//
        // Print positions every second
        // scene.printPositions();
//
        // Sleep a bit to avoid burning CPU (simulate frame time)
        // std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // ~60 updates per second
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

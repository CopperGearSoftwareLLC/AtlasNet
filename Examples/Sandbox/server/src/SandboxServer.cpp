#include "SandboxServer.hpp"
#include "AtlasNetServer.hpp"


void SandboxServer::Run()
{
    AtlasNetServer::InitializeProperties InitProperties;
    InitProperties.OnShutdownRequest = [&](SignalType signal)
    { ShouldShutdown = true; };
    AtlasNetServer::Get().Initialize(InitProperties);

    using clock = std::chrono::high_resolution_clock;
    auto previous = clock::now();

    while (!ShouldShutdown)
    {
        auto now = clock::now();
        std::chrono::duration<float> delta = now - previous;
        previous = now;
        float dt = delta.count(); // seconds

        world.Update();
        std::span<AtlasEntity> myspan;
        std::vector<AtlasEntity> Incoming;
        std::vector<AtlasEntityID> Outgoing;
        AtlasNetServer::Get().Update(myspan, Incoming, Outgoing);

        // Print positions every second
        // scene.printPositions();

        // Sleep a bit to avoid burning CPU (simulate frame time)
        // std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // ~60 updates per second
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

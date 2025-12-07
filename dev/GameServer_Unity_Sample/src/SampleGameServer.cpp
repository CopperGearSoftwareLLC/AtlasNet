#include "pch.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "AtlasNet/Client/AtlasNetClient.hpp"
#include "AtlasNet/Server/AtlasNetServer.hpp"
#include "SampleGameServer.hpp"
/**
 * @brief A simple entity that moves in a circular path.
 */
struct Entity
{
    vec2 position{0.0f, 0.0f};
    float speed; // radians per second
    float radius;
    float angle; // current angle in radians

    Entity(float radius_, float speed_)
        : speed(speed_), radius(radius_), angle(0.0f) {}

    // hard coded circular motion for testing
    void update(float dt)
    {
        angle += speed * dt;
        const float two_pi = 2.0f * 3.14159265358979323846f;
        if (angle > two_pi)
            angle -= two_pi;
        position.x = radius * std::cos(angle);
        position.y = radius * std::sin(angle);
    }
};

/**
 * @brief A simple scene managing multiple entities.
 */
class Scene
{
public:
    std::vector<Entity> entities;

    void update(float dt)
    {
        for (auto &e : entities)
        {
            e.update(dt);
        }
    }

    void printPositions() const
    {
        for (size_t i = 0; i < entities.size(); ++i)
        {
            const auto &e = entities[i];
            std::cout << "Entity[" << i << "] pos = ("
                      << e.position.x << ", " << e.position.y << ")\n";
        }
    }
};
static std::unique_ptr<AtlasNetClient> g_client;
bool ShouldShutdown = false;
/**
 * @brief Main function to run the sample game.
 */
std::string exePath;
int main(int argc, char **argv)
{
    exePath = argv[0];
    SampleGameServer s;
    s.Run();
    return 0;
}

void SampleGameServer::Run()
{
    std::cerr << "SampleGame Starting" << std::endl;
    AtlasNetServer::InitializeProperties InitProperties;
    //InitProperties.ExePath = exePath;
    InitProperties.OnShutdownRequest = [&](SignalType signal)
    { ShouldShutdown = true; };
    AtlasNetServer::Get().Initialize(InitProperties);

    Scene scene;
    // create an entity with radius that will cross partition boundaries
    // Using normalized coordinates [0,1], so radius of 0.3 centered at 0.5 will cross boundaries
    Entity& entity = scene.entities.emplace_back(0.3f, 0.5f); // radius = 0.3 (in normalized space), speed = 0.5 radians/sec
    entity.radius = 3;

    // Time / loop variables
    using clock = std::chrono::high_resolution_clock;
    auto previous = clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    while (!ShouldShutdown)
    {
        auto now = clock::now();
        std::chrono::duration<float> delta = now - previous;
        previous = now;
        float dt = delta.count(); // seconds

        scene.update(dt);

        // Update AtlasEntity with position from the moving circle entity
        std::vector<AtlasEntity> entities;
        if (!scene.entities.empty())
        {
            const auto& circleEntity = scene.entities[0];
            AtlasEntity entity;
            entity.IsPlayer = false;
            entity.ID = 0;
            entity.IsSpawned = true;
            // Map circle position to AtlasEntity Position
            // Circle is centered at origin, but we want it centered at 0.5, 0.5 in normalized space
            // Partition checks Position.x and Position.z (not y)
            entity.Position.x = 0.5f + circleEntity.position.x; // Center at 0.5 and add circle offset
            entity.Position.y = 0.0f; // Height (not used for boundary checking)
            entity.Position.z = 0.5f + circleEntity.position.y; // Center at 0.5 and add circle offset
            entity.Position.x = std::max(0.0f, std::min(1.0f, entity.Position.x)); // Clamp to [0,1]
            entity.Position.z = std::max(0.0f, std::min(1.0f, entity.Position.z)); // Clamp to [0,1]
            entities.push_back(entity);
        }
        
        std::span<AtlasEntity> myspan(entities);
        std::vector<AtlasEntity> Incoming;
        std::vector<AtlasEntityID> Outgoing;
        AtlasNetServer::Get().Update(myspan, Incoming, Outgoing);

        // Print positions every second
        // scene.printPositions();

        // Sleep a bit to avoid burning CPU (simulate frame time)
        //std::this_thread::sleep_for(std::chrono::milliseconds(16));
        // ~60 updates per second
        std::this_thread::sleep_for(std::chrono::milliseconds(32));
        // ~30 updates per second
    }

}
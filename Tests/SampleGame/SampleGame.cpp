#include "pch.hpp"
#include "AtlasNet/AtlasNet.hpp"
/**
 * @brief A simple entity that moves in a circular path.
 */
struct Entity {
    vec2 position{0.0f, 0.0f};
    float speed;   // radians per second
    float radius;
    float angle;   // current angle in radians

    Entity(float radius_, float speed_) 
      : speed(speed_), radius(radius_), angle(0.0f) {}

    // hard coded circular motion for testing
    void update(float dt) {
        angle += speed * dt;
        const float two_pi = 2.0f * 3.14159265358979323846f;
        if (angle > two_pi) angle -= two_pi;
        position.x = radius * std::cos(angle);
        position.y = radius * std::sin(angle);
    }
};

/**
 * @brief A simple scene managing multiple entities.
 */
class Scene {
public:
    std::vector<Entity> entities;

    void update(float dt) {
        for (auto & e : entities) {
            e.update(dt);
        }
    }

    void printPositions() const {
        for (size_t i = 0; i < entities.size(); ++i) {
            const auto & e = entities[i];
            std::cout << "Entity[" << i << "] pos = (" 
                      << e.position.x << ", " << e.position.y << ")\n";
        }
    }
};

/**
 * @brief Main function to run the sample game.
 */
int main() {
    AtlasNetServer::InitializeProperties InitProperties;
    AtlasNetServer::Get().Initialize(InitProperties);

    Scene scene;
    // create an entity
    scene.entities.emplace_back(10.0f, 0.5f); // radius =10, slower

    // Time / loop variables
    using clock = std::chrono::high_resolution_clock;
    auto previous = clock::now();

    while (true) 
    {
        std::span<AtlasEntity> myspan;
        std::vector<AtlasEntity> Incoming;
        std::vector<AtlasEntityID> Outgoing;
         AtlasNetServer::Get().Update(myspan,Incoming,Outgoing);
        auto now = clock::now();
        std::chrono::duration<float> delta = now - previous;
        previous = now;
        float dt = delta.count();  // seconds

        scene.update(dt);

        // Print positions every second
        //scene.printPositions();


        // Sleep a bit to avoid burning CPU (simulate frame time)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  
        // ~60 updates per second
    }

    return 0;
}

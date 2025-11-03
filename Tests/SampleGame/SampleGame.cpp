// Minimal FTXUI visualization of a single partition with a moving dot entity.
// This guts the previous sample loop and renders a simple animated view.

#include "pch.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "Debug/Log.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <sstream>
#include <iomanip>
#include <unordered_map>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

struct MovingEntity
{
    float angle = 0.0f; // radians
    float speed = 0.8f; // radians per second
};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // Map/partition dimensions in terminal canvas pixels (approximate cells)
    const int map_width = 80;
    const int map_height = 24;

    // Local simulation state for our demo entity (ID 1). We will only
    // simulate it while we own it; otherwise we display the last authoritative state.
    constexpr AtlasEntityID kDemoEntityId = 1;
    MovingEntity entity;
    std::unordered_map<AtlasEntityID, AtlasEntity> local_entities; // what we render
    bool owns_demo_entity = true; // whether this partition currently owns ID 1

    // Time step
    const float dt = 1.0f / 60.0f; // 60 FPS tick

    // Screen setup
    auto screen = ScreenInteractive::Fullscreen();
    std::atomic<bool> running = true;

    // Initialize AtlasNet server side for snapshot forwarding to Partition
    AtlasNetServer::InitializeProperties initProps;
    initProps.ExePath = argv ? argv[0] : "SampleGame";
    initProps.OnShutdownRequest = [&](SignalType) {
        running = false;
        screen.Exit();
    };
    AtlasNetServer::Get().Initialize(initProps);

    // Mute logging to avoid interfering with the FTXUI canvas.
    Log::SetMuted(true);

    // Renderer draws a bordered partition and all local entities as dots, plus a log panel.
    auto renderer = Renderer([&]
    {
        Canvas c(map_width, map_height);

        // Draw partition border.
        for (int x = 0; x < map_width; ++x)
        {
            c.DrawPoint(x, 0, true);
            c.DrawPoint(x, map_height - 1, true);
        }
        for (int y = 0; y < map_height; ++y)
        {
            c.DrawPoint(0, y, true);
            c.DrawPoint(map_width - 1, y, true);
        }

        // Draw all known local entities from their world positions.
        for (const auto &kv : local_entities)
        {
            const AtlasEntity &e = kv.second;
            c.DrawPoint((int)std::round(e.Position.x), (int)std::round(e.Position.z), true);
        }

        Element canvas_el = canvas(std::move(c));
        Element legend = hbox({
            text(" Single Partition ") | bold,
            separator(),
            text(" Dot = Entity "),
            separator(),
            text(" Ctrl+C to quit ")
        });

        // Build a small info box showing the current demo entity position, if present.
        std::ostringstream x_ss;
        std::ostringstream z_ss;
        x_ss.setf(std::ios::fixed); z_ss.setf(std::ios::fixed);
        auto it_demo = local_entities.find(kDemoEntityId);
        if (it_demo != local_entities.end())
        {
            x_ss << std::setprecision(2) << "x: " << it_demo->second.Position.x;
            z_ss << std::setprecision(2) << "z: " << it_demo->second.Position.z;
        }
        else
        {
            x_ss << "x: -";
            z_ss << "z: -";
        }
        Element pos_box = vbox({
            text(" Entity Info ") | bold,
            separator(),
            vbox({
                text("Entity Position"),
                text(x_ss.str()),
                text(z_ss.str())
            })
        }) | border | size(WIDTH, EQUAL, 30);

        return vbox({
            legend | center,
            hbox({
                canvas_el | border | size(WIDTH, EQUAL, map_width + 2) | size(HEIGHT, EQUAL, map_height + 2),
                separator(),
                pos_box
            })
        });
    });

    // We avoid CatchEvent (may be unavailable). Advance state in ticker thread
    // and just use renderer as the root component.
    auto app = renderer;

    // Seed local map with our demo entity so it's visible immediately.
    {
        AtlasEntity seed{};
        seed.ID = kDemoEntityId;
        seed.IsSpawned = true;
        seed.Position = { (float)((map_width - 1) / 2.0f), 0.0f, (float)((map_height - 1) / 2.0f) };
        local_entities[seed.ID] = seed;
    }

    // Background ticker posting animation events.
    std::thread ticker([&]
    {
        using namespace std::chrono_literals;
        std::vector<AtlasEntity> incoming;
        std::vector<AtlasEntityID> outgoing_ids;
        while (running)
        {
            // Advance local simulation only if we own the demo entity.
            if (owns_demo_entity)
            {
                entity.angle += entity.speed * dt;
                const float two_pi = 2.0f * 3.14159265358979323846f;
                if (entity.angle > two_pi)
                    entity.angle -= two_pi;

                // Update our demo entity's world position (x,z mapped to canvas space).
                const float cx = (map_width - 1) / 2.0f;
                const float cy = (map_height - 1) / 2.0f;
                const float rx = (map_width - 4) / 2.0f;
                const float ry = (map_height - 4) / 2.0f;
                const float x = cx + rx * std::cos(entity.angle);
                const float z = cy + ry * std::sin(entity.angle);

                AtlasEntity &e = local_entities[kDemoEntityId];
                e.ID = kDemoEntityId;
                e.IsSpawned = true;
                e.Position.x = x;
                e.Position.y = 0.0f;
                e.Position.z = z;
            }

            // Publish snapshot of owned entities only (here only the demo entity when owned).
            std::vector<AtlasEntity> snapshot;
            if (owns_demo_entity)
            {
                snapshot.push_back(local_entities[kDemoEntityId]);
            }
            std::span<AtlasEntity> snapshot_span(snapshot);
            AtlasNetServer::Get().Update(snapshot_span, incoming, outgoing_ids);

            // Apply authoritative changes from the network.
            for (const AtlasEntity &e : incoming)
            {
                local_entities[e.ID] = e;
                if (e.ID == kDemoEntityId)
                {
                    owns_demo_entity = true; // we just adopted it
                }
            }
            for (AtlasEntityID id : outgoing_ids)
            {
                local_entities.erase(id);
                if (id == kDemoEntityId)
                {
                    owns_demo_entity = false; // we handed it off
                }
            }

            // Request redraw after state update
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(16ms); // ~60 FPS
        }
    });

    screen.Loop(app);

    running = false;
    if (ticker.joinable())
        ticker.join();

    return 0;
}

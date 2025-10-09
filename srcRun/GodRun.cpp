#include "Globals.hpp"
#include "God/God.hpp"
#include "Partition/Partition.hpp"
#include "pch.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerEvents.hpp"

void spawnDatabaseSimple() {
    Json createBody = {
        {"Image", "database"},
        {"Cmd", {"database-redis", "--appendonly", "yes"}},
        {"HostConfig", {
            {"NetworkMode", "AtlasNet"},
            {"PortBindings", {
                {"6379/tcp", Json::array({ Json{{"HostPort", "6379"}} }) }
            }}
        }},
        {"NetworkingConfig", {
            {"EndpointsConfig", { {"AtlasNet", Json::object()} }}
        }},
        {"Name", "database-redis"}
    };

    try {
        // Remove existing one if it exists
        DockerIO::Get().request("DELETE", "/containers/database-redis?force=true");

        // Create container
        DockerIO::Get().request("POST", "/containers/create?name=database-redis", &createBody);

        // Start container
        DockerIO::Get().request("POST", "/containers/database-redis/start");
    } catch (const std::exception &e) {
    }
}


int main(int argc, char **argv)
{
    CrashHandler::Get().Init(argv[0]);
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
                                              { God::Get().Shutdown(); }});

    // quick test of spinning up the database container
    spawnDatabaseSimple();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //God &god = God::Get();
    God::Get().Init();
/*
    int32 port = 7000;
    // Example: spawn 4 partitions
    for (int32 i = 1; i <= 4; i++)
    {
        ++port;
        god.spawnPartition();
    }

    // std::this_thread::sleep_for(std::chrono::seconds(4));
    // god.removePartition(4);

    for (size_t i = 4; i < 8; i++)
    {
        ++port;
        god.spawnPartition();
    }
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cerr << "Hello\n";
    }
    return 0;*/
}

/*
int main(int argc, char **argv) {
    std::cerr << "Hello\n";
    std::cerr << "From\n";
    std::cerr << "God\n";

    for (int i = 0; i < argc; i++) {
    std::cerr << argv[i] << std::endl;
    }


}
*/
#include "AtlasNet/AtlasNetBootstrap.hpp"
#include "Debug/Crash/CrashHandler.hpp"

int main(int argc, char **argv)
{
    CrashHandler::Get().Init(argv[0]);
    AtlasNetBootstrap::Get()
        .Run();
}
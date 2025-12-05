#include "Globals.hpp"
#include "God/God.hpp"
#include "Partition/Partition.hpp"
#include "pch.hpp"
#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerEvents.hpp"

int main(int argc, char **argv)
{
    CrashHandler::Get().Init();
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
                                              { God::Get().Shutdown(); }});
    God::Get().Init();
  
}

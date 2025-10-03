#include "Partition/Partition.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "pch.hpp"
#include <Debug/Crash/CrashHandler.hpp>
#include <Docker/DockerEvents.hpp>
int main(int argc, char **argv)
{
    CrashHandler::Get().Init(argv[0]);
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
                                              { Partition::Get().Shutdown(); }});
    Partition::Get().Init();
}

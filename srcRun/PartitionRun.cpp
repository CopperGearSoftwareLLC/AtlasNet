#include "Partition/Partition.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "pch.hpp"
#include <Debug/Crash/CrashHandler.hpp>
#include <Docker/DockerEvents.hpp>
#include <fstream>
#include <Docker/DockerIO.hpp>
int main(int argc, char **argv)
{

    CrashHandler::Get().Init();
    DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
                                              { Partition::Get().Shutdown(); }});
    Partition::Get().Init();
}

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <Debug/Crash/CrashHandler.hpp>
#include <Docker/DockerEvents.hpp>
#include <Docker/DockerIO.hpp>
#include "Partition.hpp"

int main(int argc, char **argv)
{
	CrashHandler::Get().Init();
	DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
											  { Partition::Get().Shutdown(); }});
	Partition::Get().Init();
}

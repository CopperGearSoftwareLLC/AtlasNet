#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <Crash/CrashHandler.hpp>
#include <DockerEvents.hpp>
#include <DockerIO.hpp>
#include "Partition.hpp"

int main(int argc, char **argv)
{
	CrashHandler::Get().Init();
	DockerEvents::Get().Init(DockerEventsInit{.OnShutdownRequest = [](SignalType signal)
											  { Partition::Get().Shutdown(); }});
	Partition::Get().Init();
}

#include "Debug/Crash/CrashHandler.hpp"
#include "Docker/DockerEvents.hpp"
#include "WatchDog.hpp"
#include "Global/pch.hpp"

int main(int argc, char **argv)
{
	CrashHandler::Get().Init();
	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = [](SignalType signal) { WatchDog::Get().Shutdown(); }});
	WatchDog::Get().Run();
}

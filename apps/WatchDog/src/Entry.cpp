#include "Crash/CrashHandler.hpp"
#include "DockerEvents.hpp"
#include "God.hpp"
#include "pch.hpp"

int main(int argc, char **argv)
{
	CrashHandler::Get().Init();
	DockerEvents::Get().Init(
		DockerEventsInit{.OnShutdownRequest = [](SignalType signal) { God::Get().Shutdown(); }});
	God::Get().Init();
}

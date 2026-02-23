#pragma once
#include "Debug/Crash/CrashHandler.hpp"
#include "Events/EventSystem.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "NodeJSWrapper.hpp"
class NodeJSWrapper
{
   public:
	NodeJSWrapper()
	{
		CrashHandler::Get().Init();
		NetworkCredentials::Make(NetworkIdentity::MakeIDCartograph());
		Interlink::Get().Init();
		EventSystem::Get().Init();
		NetworkManifest::Get().ScheduleNetworkPings();
	}
	~NodeJSWrapper() { Interlink::Get().Shutdown(); }
};
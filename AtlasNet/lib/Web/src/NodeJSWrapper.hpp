#pragma once
#include "Events/EventSystem.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Telemetry/NetworkManifest.hpp"
#include "NodeJSWrapper.hpp"
class NodeJSWrapper
{
   public:
	NodeJSWrapper()
	{
		const auto ID = NetworkIdentity::MakeIDCartograph();
		Interlink::Get().Init(InterlinkProperties{.ThisID = ID});
		EventSystem::Get().Init(ID);
		NetworkManifest::Get().ScheduleNetworkPings(ID);
	}
	~NodeJSWrapper() { Interlink::Get().Shutdown(); }
};
#pragma once

#include <atomic>

#include "AtlasNetServer.hpp"
#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
class RTSServer : public Singleton<RTSServer>, public IAtlasNetServer
{
	Log logger = Log("RTSServer");
	std::atomic_bool ShouldShutdown = false;
   public:
	RTSServer();
	void Run();

   private:
	void OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
					   AtlasEntityPayload& payload) override
	{
		logger.DebugFormatted("Client {} joining", UUIDGen::ToString(c.client.ID));
	}
};
#pragma once

#include <atomic>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Entities/Camera.hpp"
#include "Entities/CameraPivot.hpp"
#include "EntityID.hpp"
#include "Global/Misc/Singleton.hpp"
class RTSClient : public Singleton<RTSClient>
{
	Log logger = Log("RTSClient");
	ClientID AtlasNetClientID;
	std::atomic_bool ShouldShutdown = false;
	Camera* camera;
	CameraPivot* cameraPivot;
	vec3 WorkerSelectedColor = vec3(0.6,0.2,0.4);
	std::unordered_set<EntityID> SelectedWorkers;
   public:
	void Run(const IPAddress &address);

	

   private:
	void StartupGL();
	void Render(float DeltaTime);
	void Update(float DeltaTime);
	void FixedUpdate(float FixedStep);
};

#pragma once

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Command/NetCommand.hpp"
#include "Debug/Log.hpp"
#include "Entities/Camera.hpp"
#include "Entities/CameraPivot.hpp"
#include "Entities/WorkerEntity.hpp"
#include "EntityID.hpp"
#include "Global/Misc/Singleton.hpp"
#include "PlayerColors.hpp"
#include "commands/GameStateCommand.hpp"
#include "commands/PlayerAssignStateCommand.hpp"
class RTSClient : public Singleton<RTSClient>
{
	Log logger = Log("RTSClient");
	ClientID AtlasNetClientID;
	std::atomic_bool ShouldShutdown = false;
	Camera* camera;
	CameraPivot* cameraPivot;
	vec3 WorkerSelectedColor = vec3(0.6, 0.2, 0.4);
	std::unordered_set<EntityID> SelectedWorkers;

	std::optional<PlayerTeams> myAssignedTeam;
	std::unordered_map<EntityID, EntityID> RemoteID2LocalID;
	std::unordered_map<EntityID, EntityID> LocalID2RemoteID;
	std::vector<WorkerData> WorkersToParse;
   public:
	void Run(const IPAddress& address);

   private:
	void StartupGL();
	void Render(float DeltaTime);
	void Update(float DeltaTime);
	void FixedUpdate(float FixedStep);

	void InputLogic();

	void RenderScreenText(const std::string_view text, vec4 color);
	void OnPlayerAssignStateCommand(const NetServerStateHeader& header,
									const PlayerAssignStateCommand& command);
	void onGameStateCommand(const NetServerStateHeader& header, const GameStateCommand& command);
};

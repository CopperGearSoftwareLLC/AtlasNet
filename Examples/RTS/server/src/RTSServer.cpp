
#include "RTSServer.hpp"

#include <chrono>
#include <execution>
#include <future>
#include <glm/ext/scalar_constants.hpp>
#include <iostream>
#include <iterator>
#include <numbers>
#include <thread>

#include "AtlasNetServer.hpp"
#include "Database.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "GameData.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "PlayerData.hpp"
#include "commands/GameStateCommand.hpp"
#include "commands/PlayerAssignStateCommand.hpp"
#include "commands/PlayerCameraMoveCommand.hpp"
int main()
{
	RTSServer::Get().Run();
}
RTSServer::RTSServer() {}
void RTSServer::Run()
{
	AtlasNet_Initialize();
	while (!BoundLeaser::Get().HasBound())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	GetCommandBus().Subscribe<PlayerCameraMoveCommand>(
		[this](const auto& recvHeader, const auto& command)
		{ OnClientCameraMoveCommand(recvHeader, command); });
	using Clock = std::chrono::steady_clock;

	auto lastTime = Clock::now();
	auto fpsTimer = Clock::now();

	constexpr double TargetFPS = 20.0;
	constexpr std::chrono::duration<double> TargetFrameTime(1.0 / TargetFPS);

	constexpr float MinX = -100.f;
	constexpr float MaxX = 100.f;
	constexpr float MinY = -100.f;
	constexpr float MaxY = 100.f;
	if (BoundLeaser::Get().GetBoundID() == 0)
	{
		AtlasNet_CreateEntity(AtlasTransform{});
	}
	uint64_t frameCount = 0;
	double accumulatedTime = 0.0;
	while (!ShouldShutdown)
	{

		const auto frameStart = Clock::now();

		const std::chrono::duration<double> delta = frameStart - lastTime;
		lastTime = frameStart;

		const double dt = delta.count();  // seconds
		accumulatedTime += dt;
		frameCount++;

		// ---- FPS print every second ----
		const auto now = Clock::now();
		const std::chrono::duration<double> fpsElapsed = now - fpsTimer;

		if (fpsElapsed.count() >= 1.0)
		{
			const double avgFPS = frameCount / accumulatedTime;
			// std::cout << "Active FPS: " << avgFPS << std::endl;

			frameCount = 0;
			accumulatedTime = 0.0;
			fpsTimer = now;
		}

		// ---- Frame limiting to 60 FPS ----
		const auto frameEnd = Clock::now();
		const std::chrono::duration<double> frameTime = frameEnd - frameStart;

		if (frameTime < TargetFrameTime)
		{
			std::this_thread::sleep_for(TargetFrameTime - frameTime);
		}
		SendGameStateData();
		GetCommandBus().Flush();
	}
}

void RTSServer::Shutdown()
{
	ShouldShutdown = true;
	IAtlasNetServer::Shutdown();
}

void RTSServer::OnClientCameraMoveCommand(const NetClientIntentHeader& header,
										  const PlayerCameraMoveCommand& c)
{
	if (!EntityLedger::Get().ExistsEntity(header.entityID))
	{
		logger.ErrorFormatted("GameClient Input command THROWN AWAY. THIS SHOULD NOT HAPPEN");
		return;
	}
	EntityLedger::Get().GetEntity(
		header.entityID, [&](AtlasEntity& e) { e.transform.position = c.NewCameraLocation; });
}
void RTSServer::OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
							  AtlasEntityPayload& payload)
{
	logger.DebugFormatted("Client {} joining", UUIDGen::ToString(c.client.ID));

	PlayerData playerData;
	playerData.playerColor = Database::Get().PickPlayerTeam(c.client.ID);
	ByteWriter bw;
	playerData.Serialize(bw);
	payload.assign(bw.as_string_view().begin(), bw.as_string_view().end());
	PlayerAssignStateCommand assignCommand;
	assignCommand.yourTeam = playerData.playerColor;
	GetCommandBus().Dispatch(c.client.ID, assignCommand);
	const uint32_t WorkersPerClientOnStart = 20;
	float radius = 10.0f;

	for (uint32_t i = 0; i < WorkersPerClientOnStart; i++)
	{
		float progress = (float)i / WorkersPerClientOnStart;
		float angle = progress * 2.0f * std::numbers::pi;

		vec3 workerPos = radius * vec3(cos(angle), sin(angle), 0);

		AtlasTransform t;
		t.position = entity.transform.position + workerPos;

		AtlasNet_CreateEntity(t);
	}
}
void RTSServer::SendGameStateData()
{
	std::vector<AtlasEntityHandle> allEntities;

	GlobalEntityLedger::Get().GetAllEntities(std::back_inserter(allEntities));
	logger.DebugFormatted("Sending GameState update to clients. Entity count: {}",
						  allEntities.size());
	GameStateCommand command;


	logger.DebugFormatted("Got {} Worker datas", command.Workers.size());
	for (const auto& w : command.Workers)
	{
		logger.DebugFormatted("{}", glm::to_string(w.Position));
	}
	EntityLedger::Get().ForEachClientRead(std::execution::seq,
										  [](const AtlasEntity& e) {

										  });
}

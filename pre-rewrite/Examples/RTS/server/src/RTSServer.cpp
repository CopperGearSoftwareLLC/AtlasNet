
#include "RTSServer.hpp"

#include <chrono>
#include <execution>
#include <future>
#include <glm/ext/scalar_constants.hpp>
#include <iostream>
#include <iterator>
#include <mutex>
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
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Packet/WorkerMoveNotify.hpp"
#include "Packet/WorkerRequestPacket.hpp"
#include "PlayerData.hpp"
#include "commands/GameStateCommand.hpp"
#include "commands/PlayerAssignStateCommand.hpp"
#include "commands/PlayerCameraMoveCommand.hpp"
#include "commands/WorkerMoveCommand.hpp"
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
	GetCommandBus().Subscribe<WorkerMoveCommand>([this](const auto& recvHeader, const auto& command)
												 { OnWorkerMoveCommand(recvHeader, command); });
	workerRequestSubscription = Interlink::Get().GetPacketManager().Subscribe<WorkerRequestPacket>(
		[this](const auto& packet, const auto& info) { OnWorkerRequest(packet, info); });

	WorkerMoveNotifySubscription = Interlink::Get().GetPacketManager().Subscribe<WorkerMoveNotify>(
		[this](const auto& packet, const auto& info) { OnWorkerMoveNotify(packet, info); });
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
	const uint32_t WorkersPerClientOnStart = 25;
	// Random generator setup (do this once, not per frame ideally)
	static std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

	for (uint32_t i = 0; i < WorkersPerClientOnStart; i++)
	{
		vec3 workerPos = vec3(dist(rng),  // X in [-100, 100]
							  dist(rng),  // Y in [-100, 100]
							  0.0f);

		AtlasTransform t;
		t.position = workerPos;

		WorkerData wd;
		wd.Position = t.position;
		wd.ID = UUIDGen::Gen();

		ByteWriter workerPayload;
		wd.Serialize(workerPayload);

		AtlasNet_CreateEntity(t, workerPayload.bytes());
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

		{
			std::unique_lock lock(localWorkersMutex);
			localWorkers.clear();
			EntityLedger::Get().ForEachEntityWrite(
				std::execution::seq,
				[this](AtlasEntity& e)
				{
					if (e.IsClient)
						return;
					if (e.payload.empty())
						return;

					WorkerData worker;
					ByteReader br(e.payload);
					worker.Deserialize(br);

					{
						std::lock_guard lock(unparsedTargetsMutex);
						if (UnparsedWorkerTargets.contains(worker.ID))
						{
							worker.TargetPosition = UnparsedWorkerTargets[worker.ID];
							worker.InitialPos = worker.Position;
							worker.InMotion = true;
							worker.MoveProgress = 0.0f;
							UnparsedWorkerTargets.erase(worker.ID);
							logger.DebugFormatted(
								"Worker {} starting move from ({:.2f}, {:.2f}) to ({:.2f}, {:.2f})",
								UUIDGen::ToString(worker.ID), worker.Position.x, worker.Position.y,
								worker.TargetPosition.x, worker.TargetPosition.y);
						};
					}

					if (worker.InMotion)
					{
						// simple linear movement logic for demonstration
						vec3 direction = worker.TargetPosition - worker.InitialPos;
						float distance = glm::length(direction);
						if (distance > 0.01f)
						{
							direction = glm::normalize(direction);
							float moveStep =
								std::min(3.0f, distance * (1.0f - worker.MoveProgress));
							worker.Position += direction * moveStep;
							worker.MoveProgress += moveStep / distance;
							if (worker.MoveProgress >= 1.0f)
							{
								worker.Position = worker.TargetPosition;
								worker.InMotion = false;
							}
						}
						logger.DebugFormatted(
							"Worker {} moving. Current position: ({:.2f}, {:.2f}), Progress: "
							"{:.2f}%",
							UUIDGen::ToString(worker.ID), worker.Position.x, worker.Position.y,
							worker.MoveProgress * 100.0f);
						e.transform.position = worker.Position;
					}
					ByteWriter bw;
					worker.Serialize(bw);
					e.payload.assign(bw.as_string_view().begin(), bw.as_string_view().end());
					localWorkers.push_back(worker);
				});
		}

		for (const auto& server : ServerRegistry::Get().GetServers())
		{
			if (server.second.identifier.Type == NetworkIdentityType::eShard)
			{
				RemoteShardData& shardData = remoteShards[server.second.identifier.ID];

				if (!shardData.waitingOnResponse)
				{
					WorkerRequestPacket request;
					request.state = WorkerRequestPacket::State::eRequest;
					Interlink::Get().SendMessage(server.first, request,
												 NetworkMessageSendFlag::eReliableBatched);
					shardData.waitingOnResponse = true;
				}
			}
		}
	}
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
}
void RTSServer::SendGameStateData()
{
	/* logger.DebugFormatted("Sending GameState update to clients. Worker count: {}",
						  localWorkers.size()); */
	GameStateCommand command;
	command.Workers.assign(localWorkers.begin(), localWorkers.end());
	for (const auto& shardData : remoteShards)
	{
		if (!shardData.second.waitingOnResponse)
		{
			command.Workers.insert(command.Workers.end(), shardData.second.workers.begin(),
								   shardData.second.workers.end());
		}
	}
	EntityLedger::Get().ForEachClientRead(std::execution::par, [&](const AtlasEntity& c)
										  { GetCommandBus().Dispatch(c.Client_ID, command); });
	GetCommandBus().Flush();
}
void RTSServer::OnWorkerRequest(const WorkerRequestPacket& request,
								const PacketManager::PacketInfo& info)
{
	RemoteShardData& shardData = remoteShards[info.sender.ID];
	if (request.state == WorkerRequestPacket::State::eResponse)
	{
		shardData.workers.assign(request.Workers.begin(), request.Workers.end());
		shardData.waitingOnResponse = false;
		/* logger.DebugFormatted("Received Worker data response from shard {}. Worker count: {}", */
		/* 					  UUIDGen::ToString(info.sender.ID), shardData.workers.size()); */
	}
	else
	{
		std::unique_lock lock(localWorkersMutex);
		/* logger.DebugFormatted("Received Worker data request from shard {}.",
							  UUIDGen::ToString(info.sender.ID)); */

		WorkerRequestPacket response;
		response.state = WorkerRequestPacket::State::eResponse;
		response.Workers.assign(localWorkers.begin(), localWorkers.end());
		Interlink::Get().SendMessage(info.sender, response,
									 NetworkMessageSendFlag::eReliableBatched);
	}
}
void RTSServer::OnWorkerMoveCommand(const NetClientIntentHeader& header,
									const WorkerMoveCommand& command)
{
	std::lock_guard lock(unparsedTargetsMutex);
	logger.DebugFormatted("Received WorkerMoveCommand from client {}. Move count: {}",
						  UUIDGen::ToString(header.clientID), command.Moves.size());
	for (const auto& move : command.Moves)
	{
		vec3 newPos = move.TargetPos;
		std::swap(newPos.y, newPos.z);	// client-server coordinate system difference
		UnparsedWorkerTargets[move.WorkerID] = newPos;
	}

	WorkerMoveNotify ackCommand;
	for (const auto& move : command.Moves)
	{
		vec3 newPos = move.TargetPos;
		std::swap(newPos.y, newPos.z);	// client-server coordinate system difference
		ackCommand.WorkerPositions[move.WorkerID] = newPos;
	}
	for (const auto& server : ServerRegistry::Get().GetServers())
	{
		if (server.first.Type == NetworkIdentityType::eShard)
		{
			Interlink::Get().SendMessage(server.first, ackCommand,
									 NetworkMessageSendFlag::eReliableBatched);
		}
	}

}
void RTSServer::OnWorkerMoveNotify(const WorkerMoveNotify& notify,
								   const PacketManager::PacketInfo& info)
{
	std::lock_guard lock(unparsedTargetsMutex);
	for (const auto& [id, pos] : notify.WorkerPositions)
	{
		UnparsedWorkerTargets[id] = pos;
	}
}

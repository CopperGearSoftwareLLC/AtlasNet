#pragma once

#include <atomic>
#include <mutex>

#include "AtlasNetServer.hpp"
#include "Command/NetCommand.hpp"
#include "Debug/Log.hpp"
#include "GameData.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "Packet/WorkerMoveNotify.hpp"
#include "Packet/WorkerRequestPacket.hpp"
#include "commands/PlayerCameraMoveCommand.hpp"
#include "commands/WorkerMoveCommand.hpp"
class RTSServer : public Singleton<RTSServer>, public IAtlasNetServer
{
	Log logger = Log("RTSServer");
	std::atomic_bool ShouldShutdown = false;
	PacketManager::Subscription workerRequestSubscription,WorkerMoveNotifySubscription;
	struct RemoteShardData
	{
		std::atomic_bool waitingOnResponse = false;
		std::vector<WorkerData> workers;

		RemoteShardData() = default;
		RemoteShardData(const RemoteShardData& other)
		{
			waitingOnResponse.store(other.waitingOnResponse.load());
			workers = other.workers;
		}
		RemoteShardData& operator=(const RemoteShardData& other)
		{
			waitingOnResponse.store(other.waitingOnResponse.load());
			workers = other.workers;
			return *this;
		}
	};
	boost::container::flat_map<ShardID, RemoteShardData> remoteShards;
	std::vector<WorkerData> localWorkers;
	std::mutex localWorkersMutex;

	std::mutex unparsedTargetsMutex;
	std::unordered_map<EntityID, vec3> UnparsedWorkerTargets;

   public:
	RTSServer();
	void Run();

   private:
	void OnClientSpawn(const ClientSpawnInfo& c, const AtlasEntityMinimal& entity,
					   AtlasEntityPayload& payload) override;

	void OnClientCameraMoveCommand(const NetClientIntentHeader& header,
								   const PlayerCameraMoveCommand& c);

	void SendGameStateData();

	void OnWorkerRequest(const WorkerRequestPacket& request, const PacketManager::PacketInfo& info);

	void OnWorkerMoveCommand(const NetClientIntentHeader& header, const WorkerMoveCommand& command);

	void OnWorkerMoveNotify(const WorkerMoveNotify& notify, const PacketManager::PacketInfo& info);
};
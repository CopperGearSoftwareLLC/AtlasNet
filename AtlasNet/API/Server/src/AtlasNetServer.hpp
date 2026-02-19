#pragma once
#include <stop_token>
#include <thread>
#include <unordered_set>

#include "Debug/Log.hpp"
#include "Docker/DockerEvents.hpp"
#include "Entity/Entity.hpp"
#include "Global/AtlasNet.hpp"
#include "Global/AtlasNetApi.hpp"
#include "Global/AtlasNetInterface.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkIdentity.hpp"
enum KDServerRequestType
{
	Raycast,
	SphereOverlap,

};
struct KDServerRequest
{
	KDServerRequestType Type;
};
using KDServerResponseType = std::vector<std::byte>;

class ATLASNET_API AtlasNetServer : public AtlasNetInterface,
									public Singleton<AtlasNetServer>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetServer");
	std::unordered_map<AtlasEntityID, AtlasEntity> CachedEntities;
	std::unordered_set<NetworkIdentity> ConnectedClients;
	std::vector<AtlasEntity> IncomingCache;
	std::vector<AtlasEntityID> OutgoingCache;
	NetworkIdentity identity;

	std::jthread ShardLogicThread;
   public:
	AtlasNetServer(){};
	/**
	 * @brief
	 *
	 */
	struct InitializeProperties
	{
		std::function<KDServerRequestType(KDServerRequest)>
			RequestHandleFunction;
		// std::string ExePath;
		std::function<void(SignalType signal)> OnShutdownRequest;
	};
	/**
	 * @brief Initializes the AtlasNet Front end
	 *
	 */
	void Initialize(InitializeProperties& properties);

	/**
	 * @brief Update tick for AtlasNet.
	 *
	 * @param entities Your current Entity information.
	 * @param IncomingEntities Entities incoming that you must store and keep
	 * track of.
	 * @param OutgoingEntities Entity IDs of entities you should get rid of.
	 */
	void Update(std::span<AtlasEntity> entities,
				std::vector<AtlasEntity>& IncomingEntities,
				std::vector<AtlasEntityID>& OutgoingEntities);

	AtlasEntity CreateEntity();

	private:
	 void ShardLogicEntry(std::stop_token st);
};
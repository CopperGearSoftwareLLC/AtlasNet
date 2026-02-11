#pragma once
#include "Entity.hpp"
#include "AtlasNet.hpp"
#include "AtlasNetInterface.hpp"
#include "Log.hpp"
#include "DockerEvents.hpp"
#include "Interlink.hpp"
#include "Misc/Singleton.hpp"
#include "pch.hpp"
#include "AtlasNetApi.hpp"
#include <unordered_set>
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


class ATLASNET_API AtlasNetServer : public AtlasNetInterface, public Singleton<AtlasNetServer>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetServer");
	std::unordered_map<AtlasEntity::EntityID, AtlasEntity> CachedEntities;
	std::unordered_set<NetworkIdentity> ConnectedClients;
	std::vector<AtlasEntity> IncomingCache;
	std::vector<AtlasEntity::EntityID> OutgoingCache;

   public:
	AtlasNetServer(){};
	/**
	 * @brief
	 *
	 */
	struct InitializeProperties
	{
		std::function<KDServerRequestType(KDServerRequest)> RequestHandleFunction;
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
	 * @param IncomingEntities Entities incoming that you must store and keep track of.
	 * @param OutgoingEntities Entity IDs of entities you should get rid of.
	 */
	void Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity>& IncomingEntities,
				std::vector<AtlasEntity::EntityID>& OutgoingEntities);


};
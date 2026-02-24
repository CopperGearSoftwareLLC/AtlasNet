#pragma once
#include <stop_token>
#include <thread>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Docker/DockerEvents.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
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

class ATLASNET_API AtlasNetServer : public AtlasNetInterface, public Singleton<AtlasNetServer>
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
	 * @brief Syncronise entity lists with your own fuck you
	 *
	 * @param entities Your current Entity information.
	 * @param IncomingEntities Entities incoming that you must store and keep
	 * track of.
	 * @param OutgoingEntities Entity IDs of entities you should get rid of.
	 */
	void SyncEntities(std::span<const AtlasEntityID> ActiveEntities,
					  std::span<const AtlasEntityID>& ReleasedEntities,
					  std::span<const AtlasEntityHandle>& AcquiredEntities);
	/**
	Simple get local entities
	*/

	[[nodiscard]] AtlasEntityHandle CreateEntity(const Transform& t,
												 std::span<const uint8_t> metadata = {});
	[[nodiscard]] AtlasEntityHandle CreateClientEntity(ClientID c_id, const Transform& t,
													   std::span<const uint8_t> metadata = {});

   private:
	AtlasEntity Internal_CreateEntity(const Transform& t, std::span<const uint8_t> metadata = {});
	void ShardLogicEntry(std::stop_token st);
};
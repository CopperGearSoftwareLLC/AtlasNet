#pragma once
#include <stop_token>
#include <thread>
#include <unordered_set>

#include "Client/Client.hpp"
#include "Command/ServerCommandBus.hpp"
#include "Debug/Log.hpp"
#include "Docker/DockerEvents.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
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

class ATLASNET_API IAtlasNetServer : public AtlasNetInterface
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("AtlasNetServer");

	static inline IAtlasNetServer* Instance = nullptr;
	static IAtlasNetServer& GetInstance()
	{
		ASSERT(Instance, "ERROR");
		return *Instance;
	}

   public:
	IAtlasNetServer() { Instance = this; };
	/**
	 * @brief
	 *
	 */

	/**
	 * @brief Initializes the AtlasNet Front end
	 *
	 */
	void AtlasNet_Initialize();

	/**
	 * @brief Syncronise entity lists with your own fuck you
	 *
	 * @param entities Your current Entity information.
	 * @param IncomingEntities Entities incoming that you must store and keep
	 * track of.
	 * @param OutgoingEntities Entity IDs of entities you should get rid of.
	 */
	void AtlasNet_SyncEntities(std::span<const AtlasEntityID> ActiveEntities,
							   std::span<const AtlasEntityID>& ReleasedEntities,
							   std::span<const AtlasEntityHandle>& AcquiredEntities);
	/**
	Simple get local entities
	*/

	[[nodiscard]] AtlasEntityHandle AtlasNet_CreateEntity(const Transform& t,
														  std::span<const uint8_t> metadata = {});
	[[nodiscard]] AtlasEntityHandle AtlasNet_CreateClientEntity(
		ClientID c_id, const Transform& t, std::span<const uint8_t> metadata = {});
	struct ClientSpawnInfo
	{
		Client client;
		Transform spawnLocation;  // Determined by handshake service
	};
	virtual void OnClientSpawn(const ClientSpawnInfo& c) = 0;

	ServerCommandBus& GetCommandBus() { return commandbus; }

   private:
	ServerCommandBus commandbus;
	AtlasEntity Internal_CreateEntity(const Transform& t, std::span<const uint8_t> metadata = {});
	void ShardLogicEntry(std::stop_token st);
};
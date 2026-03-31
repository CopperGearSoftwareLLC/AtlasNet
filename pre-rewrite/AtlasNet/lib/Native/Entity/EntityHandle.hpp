#pragma once
#include <functional>
#include <future>
#include <variant>

#include "Debug/Log.hpp"
#include "Entity.hpp"
#include "Entity/Packet/EntityHandleFetchRequestPacket.hpp"
#include "Network/Packet/PacketManager.hpp"
class AtlasEntityHandle
{
	Log logger = Log("AtlasEntityHandle");
	AtlasEntityID id;
	mutable std::optional<AtlasEntity> EntityData;

	

	mutable std::shared_future<AtlasEntity> future;
	mutable std::optional<std::promise<AtlasEntity>> promise;
	mutable bool requestInFlight = false;

   public:
	AtlasEntityHandle(AtlasEntityID id);
	AtlasEntityHandle(const AtlasEntityHandle& o) = delete;
	AtlasEntityHandle(AtlasEntityHandle&& o) = default;

	std::shared_future<AtlasEntity> GetAsync() const;

	const AtlasEntity& Get() const;

	// std::future<bool> Call(std::function<bool(AtlasEntity&)>);

	bool IsLocal() const;
	bool IsRemote() const { return !IsLocal(); }
	const AtlasEntityID& GetID() const { return id; }

   private:

};

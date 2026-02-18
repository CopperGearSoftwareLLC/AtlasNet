#pragma once

// Packet orchestration for entity handoff messaging: subscribe, send generic
// entity payloads, and forward received handoffs into authority manager flow.

#include <memory>

#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Entity.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

class GenericEntityPacket;

class HandoffPacketManager : public Singleton<HandoffPacketManager>
{
  public:
	HandoffPacketManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Shutdown();
	void SendEntityProbe(const NetworkIdentity& target) const;
	void SendEntityHandoff(const NetworkIdentity& target,
						   const AtlasEntity& entity) const;

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void OnGenericEntityPacket(const GenericEntityPacket& packet) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	PacketManager::Subscription handoffEntitySub;
};

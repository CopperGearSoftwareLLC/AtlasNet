#pragma once

#include <memory>

#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

class HandoffEntityPacket;

class HandoffPacketManager : public Singleton<HandoffPacketManager>
{
  public:
	HandoffPacketManager() = default;

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Shutdown();
	void SendEntityProbe(const NetworkIdentity& target) const;

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void OnHandoffEntityPacket(const HandoffEntityPacket& packet) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	PacketManager::Subscription handoffEntitySub;
};

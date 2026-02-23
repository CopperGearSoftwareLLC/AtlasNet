#pragma once

// Sends and receives naive handoff packets.
// For inbound packets, it calls runtime-provided callbacks.

#include <functional>
#include <memory>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

class GenericEntityPacket;

class NH_HandoffPacketManager : public Singleton<NH_HandoffPacketManager>
{
  public:
	using OnIncomingHandoffCallback =
		std::function<void(const AtlasEntity&, const NetworkIdentity&, uint64_t)>;
	using OnPeerActivityCallback = std::function<void(const NetworkIdentity&)>;

	NH_HandoffPacketManager() = default;

	// Lifecycle
	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Shutdown();

	// Runtime callback wiring
	void SetCallbacks(OnIncomingHandoffCallback incomingCallback,
					  OnPeerActivityCallback peerActivityCallback);

	// Outbound sends
	void SendEntityProbe(const NetworkIdentity& target) const;
	void SendEntityHandoff(const NetworkIdentity& target,
						   const AtlasEntity& entity,
						   uint64_t transferTick) const;

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void OnGenericEntityPacket(const GenericEntityPacket& packet) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	PacketManager::Subscription handoffEntitySub;
	OnIncomingHandoffCallback onIncomingHandoff;
	OnPeerActivityCallback onPeerActivity;
};

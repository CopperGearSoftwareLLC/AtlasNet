// SH handoff packet manager implementation.
// Handles packet subscribe/send and dispatches inbound handoff payloads.

#include "SH_HandoffPacketManager.hpp"

#include <chrono>

#include "Entity/EntityHandoff/Packet/GenericEntityPacket.hpp"
#include "Interlink/Interlink.hpp"

namespace
{
uint64_t NowMs()
{
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
									 std::chrono::system_clock::now().time_since_epoch())
									 .count());
}
}  // namespace

void SH_HandoffPacketManager::Init(const NetworkIdentity& self,
								std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	handoffEntitySub = Interlink::Get().GetPacketManager().Subscribe<GenericEntityPacket>(
		[this](const GenericEntityPacket& packet) { OnGenericEntityPacket(packet); });
	initialized = true;
}

void SH_HandoffPacketManager::Shutdown()
{
	handoffEntitySub.Reset();
	onIncomingHandoff = nullptr;
	onPeerActivity = nullptr;
	initialized = false;
}

void SH_HandoffPacketManager::SetCallbacks(
	OnIncomingHandoffCallback incomingCallback,
	OnPeerActivityCallback peerActivityCallback)
{
	onIncomingHandoff = std::move(incomingCallback);
	onPeerActivity = std::move(peerActivityCallback);
}

void SH_HandoffPacketManager::SendEntityProbe(const NetworkIdentity& target) const
{
	if (!initialized)
	{
		return;
	}

	GenericEntityPacket packet;
	packet.sender = selfIdentity;
	packet.entity.Entity_ID = 0;
	packet.entity.transform.world = 0;
	packet.entity.transform.position = vec3(0.0F, 0.0F, 0.0F);
	packet.entity.IsClient = false;
	packet.entity.Client_ID = UUID();
	packet.protocolVersion = GenericEntityPacket::kProtocolVersion;
	packet.transferTick = 0;
	packet.sentAtMs = NowMs();
	Interlink::Get().SendMessage(target, packet, NetworkMessageSendFlag::eReliableBatched);
}

void SH_HandoffPacketManager::SendEntityHandoff(const NetworkIdentity& target,
											 const AtlasEntity& entity,
											 const uint64_t transferTimeUs) const
{
	if (!initialized)
	{
		return;
	}

	GenericEntityPacket packet;
	packet.sender = selfIdentity;
	packet.entity = entity;
	packet.protocolVersion = GenericEntityPacket::kProtocolVersion;
	packet.transferTick = transferTimeUs;
	packet.sentAtMs = NowMs();
	Interlink::Get().SendMessage(target, packet, NetworkMessageSendFlag::eReliableBatched);

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff] HANDOFF tx entity={} from={} to={} transfer_time_us={}",
			entity.Entity_ID, selfIdentity.ToString(), target.ToString(),
			transferTimeUs);
	}
}

void SH_HandoffPacketManager::OnGenericEntityPacket(
	const GenericEntityPacket& packet) const
{
	if (!initialized)
	{
		return;
	}

	if (packet.sender == selfIdentity)
	{
		return;
	}

	if (onPeerActivity)
	{
		onPeerActivity(packet.sender);
	}
	const uint64_t transferTimeUs = packet.transferTick;
	if (onIncomingHandoff)
	{
		onIncomingHandoff(packet.entity, packet.sender, transferTimeUs);
	}

	if (logger)
	{
		const uint64_t nowMs = NowMs();
		const uint64_t latencyMs = nowMs > packet.sentAtMs ? nowMs - packet.sentAtMs : 0;
		logger->WarningFormatted(
			"[EntityHandoff] HANDOFF rx entity={} from={} latency={}ms metadata={}B "
			"transfer_time_us={} protocol_v={}",
			packet.entity.Entity_ID, packet.sender.ToString(), latencyMs,
			packet.entity.Metadata.size(), transferTimeUs,
			static_cast<uint32_t>(packet.protocolVersion));
	}
}

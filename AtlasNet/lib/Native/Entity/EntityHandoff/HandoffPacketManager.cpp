// Implements entity packet send/receive handling for handoff and adoption paths.

#include "Entity/EntityHandoff/HandoffPacketManager.hpp"

#include <chrono>

#include "Entity/EntityHandoff/EntityAuthorityManager.hpp"
#include "Entity/EntityHandoff/HandoffConnectionManager.hpp"
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

void HandoffPacketManager::Init(const NetworkIdentity& self,
								std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	handoffEntitySub = Interlink::Get().GetPacketManager().Subscribe<GenericEntityPacket>(
		[this](const GenericEntityPacket& packet) { OnGenericEntityPacket(packet); });
	initialized = true;
}

void HandoffPacketManager::Shutdown()
{
	handoffEntitySub.Reset();
	initialized = false;
}

void HandoffPacketManager::SendEntityProbe(const NetworkIdentity& target) const
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
	packet.sentAtMs = NowMs();
	Interlink::Get().SendMessage(target, packet, NetworkMessageSendFlag::eReliableBatched);
}

void HandoffPacketManager::SendEntityHandoff(const NetworkIdentity& target,
											 const AtlasEntity& entity) const
{
	if (!initialized)
	{
		return;
	}

	GenericEntityPacket packet;
	packet.sender = selfIdentity;
	packet.entity = entity;
	packet.sentAtMs = NowMs();
	Interlink::Get().SendMessage(target, packet, NetworkMessageSendFlag::eReliableBatched);

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff] HANDOFF tx entity={} from={} to={}", entity.Entity_ID,
			selfIdentity.ToString(), target.ToString());
	}
}

void HandoffPacketManager::OnGenericEntityPacket(
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

	HandoffConnectionManager::Get().MarkConnectionActivity(packet.sender);
	EntityAuthorityManager::Get().OnIncomingHandoffEntity(packet.entity, packet.sender);

	if (logger)
	{
		const uint64_t nowMs = NowMs();
		const uint64_t latencyMs = nowMs > packet.sentAtMs ? nowMs - packet.sentAtMs : 0;
		logger->WarningFormatted(
			"[EntityHandoff] HANDOFF rx entity={} from={} latency={}ms metadata={}B",
			packet.entity.Entity_ID, packet.sender.ToString(), latencyMs,
			packet.entity.Metadata.size());
	}
}

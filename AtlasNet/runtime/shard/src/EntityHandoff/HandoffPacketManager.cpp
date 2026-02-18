#include "EntityHandoff/HandoffPacketManager.hpp"

#include <chrono>
#include <span>

#include "EntityHandoff/EntityAuthorityManager.hpp"
#include "EntityHandoff/HandoffConnectionManager.hpp"
#include "EntityHandoff/Packet/HandoffEntityPacket.hpp"
#include "Interlink.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

void HandoffPacketManager::Init(const NetworkIdentity& self,
								std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	handoffEntitySub =
		Interlink::Get().GetPacketManager().Subscribe<HandoffEntityPacket>(
			[this](const HandoffEntityPacket& packet)
			{ OnHandoffEntityPacket(packet); });

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] HandoffPacketManager initialized for {}",
			selfIdentity.ToString());
		logger->Debug("[EntityHandoff] Subscribed to HandoffEntityPacket");
	}
}

void HandoffPacketManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	handoffEntitySub.Reset();
	initialized = false;
	if (logger)
	{
		logger->Debug("[EntityHandoff] HandoffPacketManager shutdown");
	}
}

void HandoffPacketManager::SendEntityProbe(const NetworkIdentity& target) const
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
						 std::chrono::system_clock::now())
						 .time_since_epoch()
						 .count();

	HandoffEntityPacket packet;
	packet.sender = selfIdentity;
	packet.entity.Entity_ID = static_cast<AtlasEntity::EntityID>(now);
	packet.entity.transform.world = 0;
	packet.entity.transform.position = vec3(0.0F, 0.0F, 0.0F);
	packet.entity.transform.boundingBox = AABB3f(vec3(0.0F), vec3(1.0F));
	packet.entity.IsClient = false;
	packet.entity.Client_ID = 0;
	ByteWriter stateWriter;
	stateWriter.u16(1);	 // state schema version
	stateWriter.u16(packet.entity.transform.world);
	stateWriter.vec3(packet.entity.transform.position);
	stateWriter.u64(packet.entity.Entity_ID);
	const auto stateBytes = stateWriter.bytes();
	packet.entity.Metadata.assign(stateBytes.begin(), stateBytes.end());
	packet.sentAtMs = static_cast<uint64_t>(now);
	Interlink::Get().SendMessage(target, packet,
								 NetworkMessageSendFlag::eReliableNow);

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] Sent HandoffEntityPacket to {} (entity_id={} "
			"metadata_bytes={})",
			target.ToString(), packet.entity.Entity_ID, packet.entity.Metadata.size());
	}
}

void HandoffPacketManager::SendEntityHandoff(const NetworkIdentity& target,
											 const AtlasEntity& entity) const
{
	if (!initialized)
	{
		return;
	}

	const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
						 std::chrono::system_clock::now())
						 .time_since_epoch()
						 .count();

	HandoffEntityPacket packet;
	packet.sender = selfIdentity;
	packet.entity = entity;
	packet.sentAtMs = static_cast<uint64_t>(now);
	Interlink::Get().SendMessage(target, packet,
								 NetworkMessageSendFlag::eReliableNow);

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff] Sent HANDOFF entity packet to {} (entity_id={})",
			target.ToString(), packet.entity.Entity_ID);
	}
}

void HandoffPacketManager::OnHandoffEntityPacket(
	const HandoffEntityPacket& packet) const
{
	if (!initialized || !logger)
	{
		return;
	}

	const auto nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(
						   std::chrono::system_clock::now())
						   .time_since_epoch()
						   .count();
	const uint64_t receiveMs = static_cast<uint64_t>(nowMs);
	const uint64_t latencyMs =
		receiveMs >= packet.sentAtMs ? (receiveMs - packet.sentAtMs) : 0;

	logger->DebugFormatted(
		"[EntityHandoff] Received HandoffEntityPacket from {} "
		"(entity_id={} metadata_bytes={}) latency={}ms sentAt={} recvAt={}",
		packet.sender.ToString(), packet.entity.Entity_ID, latencyMs,
		packet.entity.Metadata.size(), packet.sentAtMs, receiveMs);

	if (!packet.entity.Metadata.empty())
	{
		const std::span<const uint8_t> metadataSpan(packet.entity.Metadata.data(),
													packet.entity.Metadata.size());
		ByteReader stateReader(metadataSpan);
		const uint16_t schemaVersion = stateReader.u16();
		const uint16_t world = stateReader.u16();
		const vec3 position = stateReader.vec3();
		const uint64_t encodedEntityId = stateReader.u64();
		logger->DebugFormatted(
			"[EntityHandoff] Parsed entity state v{} world={} pos={} "
			"encoded_entity_id={}",
			schemaVersion, world, glm::to_string(position), encodedEntityId);
	}

	HandoffConnectionManager::Get().MarkConnectionActivity(packet.sender);
	EntityAuthorityManager::Get().OnIncomingHandoffEntity(packet.entity,
														 packet.sender);
}

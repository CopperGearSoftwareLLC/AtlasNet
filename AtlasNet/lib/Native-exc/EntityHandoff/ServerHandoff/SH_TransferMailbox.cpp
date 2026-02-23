// SH transfer mailbox.
// Stores pending incoming/outgoing handoffs and applies transfer-time actions.

#include "SH_TransferMailbox.hpp"

#include <utility>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntitySimulator.hpp"
#include "Entity/EntityHandoff/Telemetry/HandoffTransferManifest.hpp"
#include "SH_EntityAuthorityTracker.hpp"
#include "SH_TelemetryPublisher.hpp"

SH_TransferMailbox::SH_TransferMailbox(const NetworkIdentity& self,
									   std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void SH_TransferMailbox::Reset()
{
	pendingIncomingByEntityId.clear();
	pendingOutgoingByEntityId.clear();
}

void SH_TransferMailbox::QueueIncoming(const AtlasEntity& entity,
									 const NetworkIdentity& sender,
									 const uint64_t transferTimeUs)
{
	pendingIncomingByEntityId[entity.Entity_ID] = SH_PendingIncomingHandoff{
		.entity = entity, .sender = sender, .transferTimeUs = transferTimeUs};

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff][SH] Received handoff entity={} from={} transfer_time_us={}",
			entity.Entity_ID, sender.ToString(), transferTimeUs);
	}
}

size_t SH_TransferMailbox::AdoptIncomingIfDue(
	const uint64_t nowUnixTimeUs, DebugEntitySimulator& debugSimulator)
{
	if (pendingIncomingByEntityId.empty())
	{
		return 0;
	}

	std::vector<AtlasEntityID> adoptedIds;
	adoptedIds.reserve(pendingIncomingByEntityId.size());
	for (const auto& [entityId, handoff] : pendingIncomingByEntityId)
	{
		if (nowUnixTimeUs < handoff.transferTimeUs)
		{
			continue;
		}
		debugSimulator.AdoptSingleEntity(handoff.entity);
		HandoffTransferManifest::Get().MarkIncomingAdopted(
			entityId, handoff.sender, selfIdentity, handoff.transferTimeUs);
		adoptedIds.push_back(entityId);
	}

	for (const auto entityId : adoptedIds)
	{
		pendingIncomingByEntityId.erase(entityId);
	}
	return adoptedIds.size();
}

void SH_TransferMailbox::AddPendingOutgoing(
	const SH_PendingOutgoingHandoff& handoff)
{
	pendingOutgoingByEntityId[handoff.entityId] = handoff;
}

size_t SH_TransferMailbox::CommitOutgoingIfDue(
	const uint64_t nowUnixTimeUs, DebugEntitySimulator& debugSimulator,
	SH_EntityAuthorityTracker& tracker,
	const SH_TelemetryPublisher& telemetryPublisher)
{
	if (pendingOutgoingByEntityId.empty())
	{
		return 0;
	}

	std::vector<AtlasEntityID> committedIds;
	committedIds.reserve(pendingOutgoingByEntityId.size());
	std::vector<AtlasEntityID> canceledIds;
	canceledIds.reserve(pendingOutgoingByEntityId.size());
	for (const auto& [entityId, handoff] : pendingOutgoingByEntityId)
	{
		if (nowUnixTimeUs < handoff.transferTimeUs)
		{
			continue;
		}
		if (!tracker.IsPassingTo(entityId, handoff.targetIdentity))
		{
			HandoffTransferManifest::Get().MarkTransferCanceled(entityId);
			canceledIds.push_back(entityId);
			continue;
		}

		debugSimulator.RemoveEntity(entityId);
		tracker.RemoveEntity(entityId);
		HandoffTransferManifest::Get().MarkOutgoingCommitted(
			entityId, selfIdentity, handoff.targetIdentity);
		committedIds.push_back(entityId);

		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff][SH] Committed outgoing handoff entity={} target={} "
				"transfer_time_us={}",
				handoff.entityId, handoff.targetIdentity.ToString(),
				handoff.transferTimeUs);
		}
	}

	for (const auto entityId : committedIds)
	{
		pendingOutgoingByEntityId.erase(entityId);
	}
	for (const auto entityId : canceledIds)
	{
		pendingOutgoingByEntityId.erase(entityId);
	}

	if (!committedIds.empty())
	{
		telemetryPublisher.PublishFromTracker(tracker);
	}

	return committedIds.size();
}

void SH_TransferMailbox::ClearPendingOutgoing()
{
	pendingOutgoingByEntityId.clear();
}

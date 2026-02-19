// SH transfer mailbox.
// Stores pending incoming/outgoing handoffs and applies transfer-tick actions.

#include "SH_TransferMailbox.hpp"

#include <utility>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityTracker.hpp"
#include "SH_TelemetryPublisher.hpp"

SH_TransferMailbox::SH_TransferMailbox(std::shared_ptr<Log> inLogger)
	: logger(std::move(inLogger))
{
}

void SH_TransferMailbox::Reset()
{
	pendingIncomingByEntityId.clear();
	pendingOutgoingByEntityId.clear();
}

void SH_TransferMailbox::QueueIncoming(const AtlasEntity& entity,
									 const NetworkIdentity& sender,
									 const uint64_t transferTick)
{
	pendingIncomingByEntityId[entity.Entity_ID] = SH_PendingIncomingHandoff{
		.entity = entity, .sender = sender, .transferTick = transferTick};

	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff][SH] Received handoff entity={} from={} transfer_tick={}",
			entity.Entity_ID, sender.ToString(), transferTick);
	}
}

size_t SH_TransferMailbox::AdoptIncomingIfDue(
	const uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator)
{
	if (pendingIncomingByEntityId.empty())
	{
		return 0;
	}

	std::vector<AtlasEntityID> adoptedIds;
	adoptedIds.reserve(pendingIncomingByEntityId.size());
	for (const auto& [entityId, handoff] : pendingIncomingByEntityId)
	{
		if (localAuthorityTick < handoff.transferTick)
		{
			continue;
		}
		debugSimulator.AdoptSingleEntity(handoff.entity);
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
	const uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator,
	NH_EntityAuthorityTracker& tracker,
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
		if (localAuthorityTick < handoff.transferTick)
		{
			continue;
		}
		if (!tracker.IsPassingTo(entityId, handoff.targetIdentity))
		{
			canceledIds.push_back(entityId);
			continue;
		}

		debugSimulator.RemoveEntity(entityId);
		tracker.RemoveEntity(entityId);
		committedIds.push_back(entityId);

		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff][SH] Committed outgoing handoff entity={} target={} "
				"transfer_tick={}",
				handoff.entityId, handoff.targetIdentity.ToString(),
				handoff.transferTick);
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

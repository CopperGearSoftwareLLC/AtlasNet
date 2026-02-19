#pragma once

#include <memory>
#include <unordered_map>

#include "SH_HandoffTypes.hpp"

class DebugEntityOrbitSimulator;
class Log;
class NH_EntityAuthorityTracker;
class SH_TelemetryPublisher;

class SH_TransferMailbox
{
  public:
	explicit SH_TransferMailbox(std::shared_ptr<Log> inLogger);

	void Reset();
	void QueueIncoming(const AtlasEntity& entity, const NetworkIdentity& sender,
					   uint64_t transferTick);
	[[nodiscard]] size_t AdoptIncomingIfDue(
		uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator);
	void AddPendingOutgoing(const SH_PendingOutgoingHandoff& handoff);
	[[nodiscard]] size_t CommitOutgoingIfDue(
		uint64_t localAuthorityTick, DebugEntityOrbitSimulator& debugSimulator,
		NH_EntityAuthorityTracker& tracker,
		const SH_TelemetryPublisher& telemetryPublisher);
	void ClearPendingOutgoing();

	[[nodiscard]] bool HasPendingIncoming() const
	{
		return !pendingIncomingByEntityId.empty();
	}

	[[nodiscard]] bool HasPendingOutgoing() const
	{
		return !pendingOutgoingByEntityId.empty();
	}

  private:
	std::shared_ptr<Log> logger;
	std::unordered_map<AtlasEntityID, SH_PendingIncomingHandoff>
		pendingIncomingByEntityId;
	std::unordered_map<AtlasEntityID, SH_PendingOutgoingHandoff>
		pendingOutgoingByEntityId;
};

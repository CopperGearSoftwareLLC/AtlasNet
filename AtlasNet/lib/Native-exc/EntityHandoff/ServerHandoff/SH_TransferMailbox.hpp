#pragma once

#include <memory>
#include <unordered_map>

#include "SH_HandoffTypes.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntitySimulator;
class Log;
class SH_EntityAuthorityTracker;
class SH_TelemetryPublisher;

// Stores pending handoffs by entity id.
// Handles "adopt incoming" and "commit outgoing" when transfer time is due.
class SH_TransferMailbox
{
  public:
	SH_TransferMailbox(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);

	// Clears all pending state.
	void Reset();

	// Adds incoming handoff to pending map.
	void QueueIncoming(const AtlasEntity& entity, const NetworkIdentity& sender,
					   uint64_t transferTimeUs);

	// Adopts all due incoming handoffs.
	[[nodiscard]] size_t AdoptIncomingIfDue(
		uint64_t nowUnixTimeUs, DebugEntitySimulator& debugSimulator);

	// Adds outgoing handoff to pending map.
	void AddPendingOutgoing(const SH_PendingOutgoingHandoff& handoff);

	// Commits all due outgoing handoffs.
	[[nodiscard]] size_t CommitOutgoingIfDue(
		uint64_t nowUnixTimeUs, DebugEntitySimulator& debugSimulator,
		SH_EntityAuthorityTracker& tracker,
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
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	std::unordered_map<AtlasEntityID, SH_PendingIncomingHandoff>
		pendingIncomingByEntityId;
	std::unordered_map<AtlasEntityID, SH_PendingOutgoingHandoff>
		pendingOutgoingByEntityId;
};

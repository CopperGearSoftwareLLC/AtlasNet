#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SH_HandoffTypes.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;
class SH_EntityAuthorityTracker;

// Finds entities that crossed local bounds.
// Sends handoff packets with an agreed absolute transfer timestamp (Unix us).
class SH_BorderHandoffPlanner
{
  public:
	struct Options
	{
		// Delay between packet send and agreed switch time.
		std::chrono::microseconds handoffDelay = std::chrono::milliseconds(60);
	};

	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger);
	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger,
							Options inOptions);

	// Plans/sends outgoing handoffs using the current Unix microsecond time.
	// Returns pending outgoing records for mailbox tracking.
	std::vector<SH_PendingOutgoingHandoff> PlanAndSendAll(
		SH_EntityAuthorityTracker& tracker,
		uint64_t nowUnixTimeUs) const;

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
};

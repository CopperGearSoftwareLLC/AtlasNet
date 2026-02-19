#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SH_HandoffTypes.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;
class NH_EntityAuthorityTracker;

class SH_BorderHandoffPlanner
{
  public:
	struct Options
	{
		uint64_t handoffLeadTicks = 6;
	};

	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger);
	SH_BorderHandoffPlanner(const NetworkIdentity& self,
							std::shared_ptr<Log> inLogger,
							Options inOptions);

	std::vector<SH_PendingOutgoingHandoff> PlanAndSendAll(
		NH_EntityAuthorityTracker& tracker,
		uint64_t localAuthorityTick) const;

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
};

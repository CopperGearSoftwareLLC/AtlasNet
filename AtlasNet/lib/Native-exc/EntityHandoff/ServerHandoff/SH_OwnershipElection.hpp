#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "Network/NetworkIdentity.hpp"

class Log;

// Picks which shard owns the test simulation stream.
// Uses one shared DB key so all shards agree.
class SH_OwnershipElection
{
  public:
	struct Options
	{
		std::chrono::seconds evalInterval = std::chrono::seconds(2);
		std::string ownerKey = "EntityHandoff:TestOwnerShard";
	};

	SH_OwnershipElection(const NetworkIdentity& self,
						 std::shared_ptr<Log> inLogger);
	SH_OwnershipElection(const NetworkIdentity& self, std::shared_ptr<Log> inLogger,
						 Options inOptions);

	// Reset local election cache/state.
	void Reset(std::chrono::steady_clock::time_point now);

	// Recompute (or reuse cached) owner result.
	bool Evaluate(std::chrono::steady_clock::time_point now);

	// Force next Evaluate() to recompute.
	void Invalidate();

	// Explicitly set local owner flag to false.
	void ForceNotOwner();

	[[nodiscard]] bool IsOwner() const { return isOwner; }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
	bool isOwner = false;
	bool ownershipEvaluated = false;
	bool hasOwnershipLogState = false;
	bool lastOwnershipState = false;
	std::chrono::steady_clock::time_point lastOwnerEvalTime;
};

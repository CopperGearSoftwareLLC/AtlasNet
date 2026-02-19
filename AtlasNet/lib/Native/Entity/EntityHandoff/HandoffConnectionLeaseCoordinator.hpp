#pragma once

// Redis-backed lease coordinator for optional anti-dupe connection initiation
// and inactivity bookkeeping across shard peer links.

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Network/NetworkIdentity.hpp"

class HandoffConnectionLeaseCoordinator
{
  public:
	static inline constexpr std::chrono::seconds kDefaultInactivityTimeout =
		std::chrono::seconds(2);
	static inline constexpr std::chrono::seconds kDefaultLeaseTtl =
		std::chrono::seconds(25);

	struct Options
	{
		bool leaseEnabled = true;
		std::chrono::seconds inactivityTimeout = kDefaultInactivityTimeout;
		std::chrono::seconds leaseTtl = kDefaultLeaseTtl;
		std::string leaseKeyPrefix = "EntityHandoff:ConnLease:";
	};

	HandoffConnectionLeaseCoordinator(const NetworkIdentity& self,
									  std::shared_ptr<Log> inLogger,
									  Options inOptions);

	void SetLeaseEnabled(bool enabled) { options.leaseEnabled = enabled; }
	bool TryAcquireOrRefreshLease(const NetworkIdentity& peer) const;
	void ReleaseLeaseIfOwned(const NetworkIdentity& peer) const;
	void MarkConnectionActivity(const NetworkIdentity& peer);
	void ReapInactiveConnections(
		std::chrono::steady_clock::time_point now,
		const std::function<void(const NetworkIdentity&, std::chrono::seconds)>&
			onInactive);
	void Clear();

	[[nodiscard]] const Options& GetOptions() const { return options; }

  private:
	[[nodiscard]] std::string BuildLeaseKey(
		const NetworkIdentity& peer) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	Options options;
	std::unordered_map<NetworkIdentity, std::chrono::steady_clock::time_point>
		lastActivityByPeer;
};

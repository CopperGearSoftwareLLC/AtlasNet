// SH handoff lease coordinator implementation.
// Handles lease acquire/refresh/release and stale-activity reaping callbacks.

#include "SH_HandoffConnectionLeaseCoordinator.hpp"

#include <string>
#include <vector>

#include "InternalDB/InternalDB.hpp"

SH_HandoffConnectionLeaseCoordinator::SH_HandoffConnectionLeaseCoordinator(
	const NetworkIdentity& self, std::shared_ptr<Log> inLogger, Options inOptions)
	: selfIdentity(self), logger(std::move(inLogger)), options(std::move(inOptions))
{
}

std::string SH_HandoffConnectionLeaseCoordinator::BuildLeaseKey(
	const NetworkIdentity& peer) const
{
	const std::string selfKey = selfIdentity.ToString();
	const std::string peerKey = peer.ToString();
	if (selfKey < peerKey)
	{
		return options.leaseKeyPrefix + selfKey + "|" + peerKey;
	}
	return options.leaseKeyPrefix + peerKey + "|" + selfKey;
}

bool SH_HandoffConnectionLeaseCoordinator::TryAcquireOrRefreshLease(
	const NetworkIdentity& peer) const
{
	if (!options.leaseEnabled)
	{
		return true;
	}

	const std::string leaseKey = BuildLeaseKey(peer);
	const std::string ownerValue = selfIdentity.ToString();
	const std::optional<std::string> currentOwner = InternalDB::Get()->Get(leaseKey);
	if (currentOwner.has_value() && *currentOwner != ownerValue)
	{
		return false;
	}

	const bool wrote = InternalDB::Get()->Set(leaseKey, ownerValue);
	(void)wrote;
	const bool ttlSet = InternalDB::Get()->Expire(leaseKey, options.leaseTtl);
	(void)ttlSet;
	return true;
}

void SH_HandoffConnectionLeaseCoordinator::ReleaseLeaseIfOwned(
	const NetworkIdentity& peer) const
{
	if (!options.leaseEnabled)
	{
		return;
	}

	const std::string leaseKey = BuildLeaseKey(peer);
	const std::optional<std::string> currentOwner = InternalDB::Get()->Get(leaseKey);
	if (currentOwner.has_value() && *currentOwner == selfIdentity.ToString())
	{
		const long long removed = InternalDB::Get()->DelKey(leaseKey);
		(void)removed;
	}
}

void SH_HandoffConnectionLeaseCoordinator::MarkConnectionActivity(
	const NetworkIdentity& peer)
{
	lastActivityByPeer[peer] = std::chrono::steady_clock::now();
	const bool acquired = TryAcquireOrRefreshLease(peer);
	(void)acquired;
}

void SH_HandoffConnectionLeaseCoordinator::ReapInactiveConnections(
	std::chrono::steady_clock::time_point now,
	const std::function<void(const NetworkIdentity&, std::chrono::seconds)>&
		onInactive)
{
	std::vector<NetworkIdentity> toRemove;
	for (const auto& [peer, lastActivity] : lastActivityByPeer)
	{
		const auto inactiveFor =
			std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity);
		if (inactiveFor >= options.inactivityTimeout)
		{
			onInactive(peer, inactiveFor);
			ReleaseLeaseIfOwned(peer);
			toRemove.push_back(peer);
		}
	}

	for (const auto& peer : toRemove)
	{
		lastActivityByPeer.erase(peer);
	}
}

void SH_HandoffConnectionLeaseCoordinator::Clear()
{
	for (const auto& [peer, _lastActivity] : lastActivityByPeer)
	{
		ReleaseLeaseIfOwned(peer);
	}
	lastActivityByPeer.clear();
}

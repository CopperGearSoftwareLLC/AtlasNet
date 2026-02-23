// SH ownership election.
// Chooses a single test owner shard and tracks local ownership state.

#include "SH_OwnershipElection.hpp"

#include <optional>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/ServerRegistry.hpp"

namespace
{
std::optional<std::string> SelectBootstrapOwner(
	const std::unordered_map<NetworkIdentity, ServerRegistryEntry>& servers)
{
	std::optional<std::string> selectedOwner;
	for (const auto& [id, _entry] : servers)
	{
		if (id.Type != NetworkIdentityType::eShard)
		{
			continue;
		}
		const std::string candidate = id.ToString();
		if (!selectedOwner.has_value() || candidate < selectedOwner.value())
		{
			selectedOwner = candidate;
		}
	}
	return selectedOwner;
}
}  // namespace

SH_OwnershipElection::SH_OwnershipElection(const NetworkIdentity& self,
										 std::shared_ptr<Log> inLogger)
	: SH_OwnershipElection(self, std::move(inLogger), Options{})
{
}

SH_OwnershipElection::SH_OwnershipElection(const NetworkIdentity& self,
										 std::shared_ptr<Log> inLogger,
										 Options inOptions)
	: selfIdentity(self),
	  logger(std::move(inLogger)),
	  options(std::move(inOptions))
{
}

void SH_OwnershipElection::Reset(const std::chrono::steady_clock::time_point now)
{
	isOwner = false;
	ownershipEvaluated = false;
	hasOwnershipLogState = false;
	lastOwnershipState = false;
	lastOwnerEvalTime = now - options.evalInterval;
}

bool SH_OwnershipElection::Evaluate(
	const std::chrono::steady_clock::time_point now)
{
	if (ownershipEvaluated && now - lastOwnerEvalTime < options.evalInterval)
	{
		return isOwner;
	}
	lastOwnerEvalTime = now;
	ownershipEvaluated = true;

	const auto& servers = ServerRegistry::Get().GetServers();
	std::optional<std::string> selectedOwner = InternalDB::Get()->Get(options.ownerKey);
	if (!selectedOwner.has_value() || selectedOwner->empty())
	{
		selectedOwner = SelectBootstrapOwner(servers);
		if (selectedOwner.has_value())
		{
			const bool ownerWritten =
				InternalDB::Get()->Set(options.ownerKey, selectedOwner.value());
			(void)ownerWritten;
		}
	}

	if (selectedOwner.has_value())
	{
		bool ownerStillValid = false;
		for (const auto& [id, _entry] : servers)
		{
			if (id.Type != NetworkIdentityType::eShard)
			{
				continue;
			}
			if (id.ToString() == selectedOwner.value())
			{
				ownerStillValid = true;
				break;
			}
		}
		if (!ownerStillValid)
		{
			selectedOwner = SelectBootstrapOwner(servers);
			if (selectedOwner.has_value())
			{
				const bool ownerWritten =
					InternalDB::Get()->Set(options.ownerKey, selectedOwner.value());
				(void)ownerWritten;
			}
		}
	}

	if (!selectedOwner.has_value())
	{
		isOwner = false;
		return false;
	}

	isOwner = selectedOwner.value() == selfIdentity.ToString();
	if (logger && (!hasOwnershipLogState || lastOwnershipState != isOwner))
	{
		hasOwnershipLogState = true;
		lastOwnershipState = isOwner;
		logger->DebugFormatted(
			"[EntityHandoff][SH] Ownership transition owner={} self={} owning={}",
			selectedOwner.value(), selfIdentity.ToString(),
			isOwner ? "yes" : "no");
	}
	return isOwner;
}

void SH_OwnershipElection::Invalidate()
{
	ownershipEvaluated = false;
}

void SH_OwnershipElection::ForceNotOwner()
{
	isOwner = false;
}

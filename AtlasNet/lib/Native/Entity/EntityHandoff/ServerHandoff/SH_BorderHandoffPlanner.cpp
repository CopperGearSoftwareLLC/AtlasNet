// SH boundary handoff planner.
// Detects entities crossing local bounds and emits a transfer intent.

#include "SH_BorderHandoffPlanner.hpp"

#include <optional>
#include <unordered_map>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/Telemetry/HandoffTransferManifest.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Network/NetworkIdentity.hpp"
#include "SH_EntityAuthorityTracker.hpp"
#include "SH_HandoffPacketManager.hpp"

namespace
{
NetworkIdentity SelectTargetClaimKeyForPosition(
	const std::unordered_map<NetworkIdentity, GridShape>& claimedBounds,
	const NetworkIdentity& selfKey, const vec3& position)
{
	for (const auto& [claimKey, bound] : claimedBounds)
	{
		if (claimKey == selfKey)
		{
			continue;
		}
		if (bound.Contains(position))
		{
			return claimKey;
		}
	}
	return {};
}

std::optional<NetworkIdentity> ResolveIdentityFromClaimKey(
	const NetworkIdentity& claimKey,
	const std::unordered_map<NetworkIdentity, ServerRegistryEntry>& servers)
{
	for (const auto& [id, _entry] : servers)
	{
		if (id == claimKey)
		{
			return id;
		}
	}
	return std::nullopt;
}
}  // namespace

SH_BorderHandoffPlanner::SH_BorderHandoffPlanner(const NetworkIdentity& self,
												 std::shared_ptr<Log> inLogger)
	: SH_BorderHandoffPlanner(self, std::move(inLogger), Options{})
{
}

SH_BorderHandoffPlanner::SH_BorderHandoffPlanner(const NetworkIdentity& self,
												 std::shared_ptr<Log> inLogger,
												 Options inOptions)
	: selfIdentity(self),
	  logger(std::move(inLogger)),
	  options(std::move(inOptions))
{
}

std::vector<SH_PendingOutgoingHandoff>
SH_BorderHandoffPlanner::PlanAndSendAll(
	SH_EntityAuthorityTracker& tracker, const uint64_t nowUnixTimeUs) const
{
	std::vector<SH_PendingOutgoingHandoff> outgoingHandoffs;
	std::unordered_map<NetworkIdentity, GridShape> claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape>(
		claimedBounds);
	if (claimedBounds.empty())
	{
		return outgoingHandoffs;
	}

	const std::string selfKey = selfIdentity.ToString();
	const auto selfIt = claimedBounds.find(selfIdentity);
	if (selfIt == claimedBounds.end())
	{
		return outgoingHandoffs;
	}

	const GridShape& selfBounds = selfIt->second;
	const auto entities = tracker.GetOwnedEntitySnapshots();
	for (const auto& entity : entities)
	{
		// Keep handoff one-way once initiated. Mailbox commit/cancel is the
		// only place that resolves a passing transfer.
		if (tracker.IsPassing(entity.Entity_ID))
		{
			continue;
		}

		const vec3 position = entity.transform.position;
		if (selfBounds.Contains(position))
		{
			tracker.MarkAuthoritative(entity.Entity_ID);
			continue;
		}

		const NetworkIdentity targetClaimKey = SelectTargetClaimKeyForPosition(
			claimedBounds, selfIdentity, position);

		const auto targetIdentity = ResolveIdentityFromClaimKey(
			targetClaimKey, ServerRegistry::Get().GetServers());
		if (!targetIdentity.has_value() || *targetIdentity == selfIdentity)
		{
			continue;
		}

		const bool shouldSendHandoff =
			tracker.MarkPassing(entity.Entity_ID, *targetIdentity);
		if (!shouldSendHandoff)
		{
			continue;
		}

		const auto delayUsCount = options.handoffDelay.count();
		const uint64_t delayUs =
			delayUsCount > 0 ? static_cast<uint64_t>(delayUsCount) : 0;
		const uint64_t transferTimeUs = nowUnixTimeUs + delayUs;
		SH_HandoffPacketManager::Get().SendEntityHandoff(*targetIdentity, entity,
														 transferTimeUs);
		HandoffTransferManifest::Get().MarkTransferStarted(
			entity.Entity_ID, selfIdentity, *targetIdentity, transferTimeUs);

		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff][SH] Triggered passing state entity={} pos={} "
				"self_bound_id={} target_claim={} target_id={} transfer_time_us={} "
				"handoff_delay_us={}",
				entity.Entity_ID, glm::to_string(position), selfBounds.GetID(),
				targetClaimKey.ToString(), targetIdentity->ToString(), transferTimeUs,
				delayUs);
		}

		outgoingHandoffs.push_back(SH_PendingOutgoingHandoff{
			.entityId = entity.Entity_ID,
			.targetIdentity = *targetIdentity,
			.transferTimeUs = transferTimeUs});
	}

	return outgoingHandoffs;
}

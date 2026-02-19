// SH boundary handoff planner.
// Detects entities crossing local bounds and emits a transfer intent.

#include "SH_BorderHandoffPlanner.hpp"

#include <optional>
#include <unordered_map>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityTracker.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_HandoffPacketManager.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Interlink/Database/ServerRegistry.hpp"

namespace
{
std::string SelectTargetClaimKeyForPosition(
	const std::unordered_map<std::string, GridShape>& claimedBounds,
	const std::string& selfKey, const vec3& position)
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
	const std::string& claimKey,
	const std::unordered_map<NetworkIdentity, ServerRegistryEntry>& servers)
{
	for (const auto& [id, _entry] : servers)
	{
		if (id.ToString() == claimKey)
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
	NH_EntityAuthorityTracker& tracker, const uint64_t localAuthorityTick) const
{
	std::vector<SH_PendingOutgoingHandoff> outgoingHandoffs;
	std::unordered_map<std::string, GridShape> claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape, std::string>(
		claimedBounds);
	if (claimedBounds.empty())
	{
		return outgoingHandoffs;
	}

	const std::string selfKey = selfIdentity.ToString();
	const auto selfIt = claimedBounds.find(selfKey);
	if (selfIt == claimedBounds.end())
	{
		return outgoingHandoffs;
	}

	const GridShape& selfBounds = selfIt->second;
	const auto entities = tracker.GetOwnedEntitySnapshots();
	for (const auto& entity : entities)
	{
		const vec3 position = entity.transform.position;
		if (selfBounds.Contains(position))
		{
			tracker.MarkAuthoritative(entity.Entity_ID);
			continue;
		}

		const std::string targetClaimKey = SelectTargetClaimKeyForPosition(
			claimedBounds, selfKey, position);
		if (targetClaimKey.empty())
		{
			continue;
		}

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

		const uint64_t transferTick =
			localAuthorityTick + options.handoffLeadTicks;
		NH_HandoffPacketManager::Get().SendEntityHandoff(*targetIdentity, entity,
														 transferTick);

		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff][SH] Triggered passing state entity={} pos={} "
				"self_bound_id={} target_claim={} target_id={} transfer_tick={}",
				entity.Entity_ID, glm::to_string(position), selfBounds.GetID(),
				targetClaimKey, targetIdentity->ToString(), transferTick);
		}

		outgoingHandoffs.push_back(SH_PendingOutgoingHandoff{
			.entityId = entity.Entity_ID,
			.targetIdentity = *targetIdentity,
			.targetClaimKey = targetClaimKey,
			.transferTick = transferTick});
	}

	return outgoingHandoffs;
}

#include "BoundLeaser.hpp"

#include <string>

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Snapshot/SnapshotService.hpp"
#include "Snapshot/TemporaryMigrationService.hpp"

void BoundLeaser::ClaimBound()
{
	const auto& SelfID = NetworkCredentials::Get().GetID();
	logger.Debug("Claiming the next pending bound");
	const std::optional<BoundsID> claimedBoundID =
		HeuristicManifest::Get().ClaimNextPendingBound(SelfID);
	{
		std::lock_guard lock(ClaimedBoundMutex);
		ClaimedBoundID = claimedBoundID;
	}
	if (!claimedBoundID.has_value())
	{
		logger.Warning("No pending bounds available yet; retrying.");
		return;
	}
	logger.DebugFormatted("Claiming bound id {}", claimedBoundID.value());
	SnapshotService::Get().RecoverClaimedBoundSnapshotIfNeeded();
	/*
	const auto boundIdInManifest = HeuristicManifest::Get().BoundIDFromShard(SelfID);
	if (!boundIdInManifest.has_value() || *boundIdInManifest != ClaimedBoundID)
	{
		logger.WarningFormatted(
			"Claimed bound {} but manifest reports {} for self shard.",
			ClaimedBoundID.value(), boundIdInManifest.has_value() ? std::to_string(*boundIdInManifest)
														 : std::string("none"));
		}*/
}

void BoundLeaser::ClearInvalidClaimedBound(BoundsID claimedBoundID)
{
	TemporaryMigrationService::Get().TriggerForCurrentShardSigterm();
	if (HasBound())
	{
		logger.WarningFormatted(
			"Temporary migration did not clear stale bound {} immediately; clearing local claim as "
			"a fallback.",
			claimedBoundID);
		ClearClaimedBound();
	}
}

void BoundLeaser::ValidateClaimedBound(BoundsID claimedBoundID)
{
	const NetworkIdentity selfID = NetworkCredentials::Get().GetID();

	try
	{
		const bool boundStillExists = HeuristicManifest::Get().PullHeuristic(
			[&](const IHeuristic& heuristic)
			{
				return static_cast<uint64_t>(claimedBoundID) <
					   static_cast<uint64_t>(heuristic.GetBoundsCount());
			});
		if (!boundStillExists)
		{
			logger.WarningFormatted(
				"Claimed bound {} is no longer present in the active heuristic. Starting "
				"temporary migration.",
				claimedBoundID);
			ClearInvalidClaimedBound(claimedBoundID);
			return;
		}

		const bool stillOwnedBySelf = HeuristicManifest::Get().QueryOwnershipState(
			[&](const HeuristicManifest::OwnershipStateWrapper& ownership)
			{
				const std::optional<NetworkIdentity> owner = ownership.GetBoundOwner(claimedBoundID);
				return owner.has_value() && owner.value() == selfID;
			});
		if (!stillOwnedBySelf)
		{
			logger.WarningFormatted(
				"Shard {} lost ownership of claimed bound {} before shutdown. Starting "
				"temporary migration immediately.",
				selfID.ToString(), claimedBoundID);
			ClearInvalidClaimedBound(claimedBoundID);
		}
	}
	catch (const std::exception& ex)
	{
		logger.WarningFormatted(
			"Failed to validate claimed bound {} for {}. Starting temporary migration. {}",
			claimedBoundID, selfID.ToString(), ex.what());
		ClearInvalidClaimedBound(claimedBoundID);
	}
	catch (...)
	{
		logger.WarningFormatted(
			"Failed to validate claimed bound {} for {}. Starting temporary migration.",
			claimedBoundID, selfID.ToString());
		ClearInvalidClaimedBound(claimedBoundID);
	}
}

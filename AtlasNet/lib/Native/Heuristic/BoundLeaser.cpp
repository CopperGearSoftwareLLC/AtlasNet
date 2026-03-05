#include "BoundLeaser.hpp"

#include <string>

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Network/NetworkCredentials.hpp"

void BoundLeaser::ClaimBound()
{
	const auto& SelfID = NetworkCredentials::Get().GetID();
	logger.Debug("Claiming the next pending bound");
	ClaimedBound =
		HeuristicManifest::Get().ClaimNextPendingBound(SelfID);
	if (!ClaimedBound)
	{
		logger.Warning("No pending bounds available yet; retrying.");
		return;
	}
	ClaimedBoundID = ClaimedBound->GetID();
	logger.DebugFormatted("Claiming bound id {}", ClaimedBoundID);

	const auto boundIdInManifest = HeuristicManifest::Get().BoundIDFromShard(SelfID);
	if (!boundIdInManifest.has_value() || *boundIdInManifest != ClaimedBoundID)
	{
		logger.WarningFormatted(
			"Claimed bound {} but manifest reports {} for self shard.",
			ClaimedBoundID, boundIdInManifest.has_value() ? std::to_string(*boundIdInManifest)
														 : std::string("none"));
	}
}

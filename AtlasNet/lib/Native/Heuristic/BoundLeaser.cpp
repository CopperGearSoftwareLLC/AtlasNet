#include "BoundLeaser.hpp"

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Network/NetworkCredentials.hpp"

void BoundLeaser::ClaimBound()
{
	const auto& SelfID = NetworkCredentials::Get().GetID();
	logger.Debug("Claiming the next pending bound");
	ClaimedBound =
		HeuristicManifest::Get().ClaimNextPendingBound(SelfID);
	ASSERT(ClaimedBound, "ClaimNextPendingBound returned null");
	ClaimedBoundID = ClaimedBound->GetID();
	logger.DebugFormatted("Claiming bound id {}", ClaimedBoundID);

	ASSERT(ClaimedBoundID == HeuristicManifest::Get().BoundIDFromShard(SelfID).value(),
		   "Internal Error");
}

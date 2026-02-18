#include "EntityHandoff/EntityAuthorityManager.hpp"

#include <optional>

#include "Database/ServerRegistry.hpp"
#include "EntityHandoff/HandoffPacketManager.hpp"
#include "EntityHandoff/HandoffConnectionManager.hpp"
#include "InternalDB.hpp"
#include "Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"

namespace
{
constexpr std::chrono::seconds kOwnershipEvalInterval(2);
constexpr std::chrono::milliseconds kStateSnapshotInterval(250);
constexpr float kOrbitRadius = 12.0F;
constexpr float kOrbitAngularSpeedRadPerSec = 1.2F;
constexpr float kTestEntityHalfExtent = 0.5F;
constexpr uint32_t kDefaultTestEntityCount = 1;
constexpr float kEntityPhaseStepRad = 0.7F;

constexpr std::string_view kTestOwnerKey = "EntityHandoff:TestOwnerShard";
constexpr std::string_view kTestEntityStateHash = "EntityHandoff:TestEntityState";
constexpr std::string_view kAuthorityStateHash = "EntityHandoff:AuthorityState";

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

	for (const auto& [claimKey, _bound] : claimedBounds)
	{
		if (claimKey == selfKey)
		{
			continue;
		}
		return claimKey;
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

void EntityAuthorityManager::Init(const NetworkIdentity& self,
								  std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	isTestEntityOwner = false;
	ownershipEvaluated = false;
	tracker = std::make_unique<EntityAuthorityTracker>(selfIdentity, logger);
	debugSimulator =
		std::make_unique<DebugEntityOrbitSimulator>(selfIdentity, logger);
	tracker->Reset();
	debugSimulator->Reset();
	pendingIncomingEntity.reset();
	lastTickTime = std::chrono::steady_clock::now();
	lastOwnerEvalTime = lastTickTime - kOwnershipEvalInterval;
	lastSnapshotTime = lastTickTime - kStateSnapshotInterval;

	HandoffConnectionManager::Get().Init(selfIdentity, logger);

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] EntityAuthorityManager initialized for {}",
			selfIdentity.ToString());
	}
}

void EntityAuthorityManager::Tick()
{
	if (!initialized)
	{
		return;
	}

	HandoffConnectionManager::Get().Tick();
	EvaluateTestEntityOwnership();
	if (isTestEntityOwner && tracker && debugSimulator)
	{
		if (pendingIncomingEntity.has_value())
		{
			debugSimulator->AdoptSingleEntity(*pendingIncomingEntity);
			pendingIncomingEntity.reset();
		}
		debugSimulator->SeedEntities(
			DebugEntityOrbitSimulator::SeedOptions{
				.desiredCount = kDefaultTestEntityCount,
				.halfExtent = kTestEntityHalfExtent,
				.phaseStepRad = kEntityPhaseStepRad});

		const auto now = std::chrono::steady_clock::now();
		const float deltaSeconds =
			std::chrono::duration<float>(now - lastTickTime).count();
		lastTickTime = now;

		debugSimulator->TickOrbit(
			DebugEntityOrbitSimulator::OrbitOptions{
				.deltaSeconds = deltaSeconds,
				.radius = kOrbitRadius,
				.angularSpeedRadPerSec = kOrbitAngularSpeedRadPerSec});
		tracker->SetOwnedEntities(debugSimulator->GetEntitiesSnapshot());
		EvaluateHeuristicPositionTriggers();
		if (now - lastSnapshotTime >= kStateSnapshotInterval)
		{
			lastSnapshotTime = now;
			tracker->StoreAuthorityStateSnapshots(kAuthorityStateHash);
			tracker->DebugLogTrackedEntities();
			debugSimulator->StoreStateSnapshots(kTestEntityStateHash);
		}
	}
	else if (tracker && debugSimulator)
	{
		tracker->SetOwnedEntities({});
		tracker->StoreAuthorityStateSnapshots(kAuthorityStateHash);
		tracker->DebugLogTrackedEntities();
		debugSimulator->Reset();
	}
}

void EntityAuthorityManager::EvaluateHeuristicPositionTriggers()
{
	if (!tracker)
	{
		return;
	}

	std::unordered_map<std::string, GridShape> claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape, std::string>(
		claimedBounds);
	if (claimedBounds.empty())
	{
		return;
	}

	const std::string selfKey = selfIdentity.ToString();
	const auto selfIt = claimedBounds.find(selfKey);
	if (selfIt == claimedBounds.end())
	{
		return;
	}
	const GridShape& selfBounds = selfIt->second;

	const auto entities = tracker->GetOwnedEntitySnapshots();
	for (const auto& entity : entities)
	{
		const vec3 position = entity.transform.position;
		if (selfBounds.Contains(position))
		{
			tracker->MarkAuthoritative(entity.Entity_ID);
			continue;
		}

		const std::string targetClaimKey = SelectTargetClaimKeyForPosition(
			claimedBounds, selfKey, position);
		if (targetClaimKey.empty())
		{
			continue;
		}
		const auto targetIdentity =
			ResolveIdentityFromClaimKey(targetClaimKey, ServerRegistry::Get().GetServers());
		if (!targetIdentity.has_value() || *targetIdentity == selfIdentity)
		{
			continue;
		}

		const bool shouldSendHandoff =
			tracker->MarkPassing(entity.Entity_ID, *targetIdentity);
		if (!shouldSendHandoff)
		{
			continue;
		}
		HandoffPacketManager::Get().SendEntityHandoff(*targetIdentity, entity);
		const bool ownerSwitched = InternalDB::Get()->Set(kTestOwnerKey, targetClaimKey);
		(void)ownerSwitched;
		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff] Triggered passing state entity={} pos={} "
				"self_bound_id={} target_claim={} target_id={}",
				entity.Entity_ID, glm::to_string(position), selfBounds.GetID(),
				targetClaimKey, targetIdentity->ToString());
		}
	}
}

void EntityAuthorityManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	HandoffConnectionManager::Get().Shutdown();
	tracker.reset();
	debugSimulator.reset();
	pendingIncomingEntity.reset();
	initialized = false;

	if (logger)
	{
		logger->Debug("[EntityHandoff] EntityAuthorityManager shutdown");
	}
}

void EntityAuthorityManager::EvaluateTestEntityOwnership()
{
	const auto now = std::chrono::steady_clock::now();
	if (ownershipEvaluated &&
		now - lastOwnerEvalTime < kOwnershipEvalInterval)
	{
		return;
	}
	lastOwnerEvalTime = now;
	ownershipEvaluated = true;

	std::string selectedOwner = selfIdentity.ToString();
	const auto& servers = ServerRegistry::Get().GetServers();
	for (const auto& [id, _entry] : servers)
	{
		if (id.Type != NetworkIdentityType::eShard)
		{
			continue;
		}
		const std::string candidate = id.ToString();
		if (candidate < selectedOwner)
		{
			selectedOwner = candidate;
		}
	}

	isTestEntityOwner = (selectedOwner == selfIdentity.ToString());
	const bool ownerWritten = InternalDB::Get()->Set(kTestOwnerKey, selectedOwner);
	(void)ownerWritten;

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff] Test owner={} self={} owning={}",
			selectedOwner, selfIdentity.ToString(),
			isTestEntityOwner ? "yes" : "no");
	}
}

void EntityAuthorityManager::OnIncomingHandoffEntity(
	const AtlasEntity& entity, const NetworkIdentity& sender)
{
	pendingIncomingEntity = entity;
	if (logger)
	{
		logger->WarningFormatted(
			"[EntityHandoff] Received handoff entity={} from {}",
			entity.Entity_ID, sender.ToString());
	}
}

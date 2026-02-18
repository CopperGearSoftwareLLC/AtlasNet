#include "EntityHandoff/EntityAuthorityManager.hpp"

#include "Database/ServerRegistry.hpp"
#include "EntityHandoff/HandoffConnectionManager.hpp"
#include "InternalDB.hpp"

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

void EntityAuthorityManager::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	HandoffConnectionManager::Get().Shutdown();
	tracker.reset();
	debugSimulator.reset();
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

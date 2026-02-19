// SH server authority runtime.
// Orchestrates ownership election, simulation, handoff planning, and transfer commit.

#include "SH_ServerAuthorityRuntime.hpp"

#include <chrono>
#include <utility>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_EntityAuthorityTracker.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_HandoffConnectionManager.hpp"
#include "Entity/EntityHandoff/NaiveHandoff/NH_HandoffPacketManager.hpp"
#include "SH_BorderHandoffPlanner.hpp"
#include "SH_OwnershipElection.hpp"
#include "SH_TelemetryPublisher.hpp"
#include "SH_TransferMailbox.hpp"

namespace
{
constexpr std::chrono::milliseconds kStateSnapshotInterval(250);
constexpr float kOrbitRadius = 50.0F;
constexpr float kOrbitAngularSpeedRadPerSec = 0.35F;
constexpr float kTestEntityHalfExtent = 0.5F;
constexpr float kEntityPhaseStepRad = 0.7F;

#ifndef ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT
#define ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT 1000
#endif
constexpr uint32_t kDefaultTestEntityCount =
	(ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT > 0)
		? static_cast<uint32_t>(ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT)
		: 1U;
}  // namespace

SH_ServerAuthorityRuntime::SH_ServerAuthorityRuntime() = default;

SH_ServerAuthorityRuntime::~SH_ServerAuthorityRuntime()
{
	if (initialized)
	{
		Shutdown();
	}
}

void SH_ServerAuthorityRuntime::Init(const NetworkIdentity& self,
									 std::shared_ptr<Log> inLogger)
{
	selfIdentity = self;
	logger = std::move(inLogger);
	initialized = true;
	hasSeededInitialEntities = false;
	localAuthorityTick = 0;
	lastTickTime = std::chrono::steady_clock::now();
	lastSnapshotTime = lastTickTime - kStateSnapshotInterval;

	tracker = std::make_unique<NH_EntityAuthorityTracker>(selfIdentity, logger);
	debugSimulator =
		std::make_unique<DebugEntityOrbitSimulator>(selfIdentity, logger);
	ownershipElection =
		std::make_unique<SH_OwnershipElection>(selfIdentity, logger);
	borderPlanner =
		std::make_unique<SH_BorderHandoffPlanner>(selfIdentity, logger);
	transferMailbox = std::make_unique<SH_TransferMailbox>(logger);
	telemetryPublisher = std::make_unique<SH_TelemetryPublisher>();

	tracker->Reset();
	debugSimulator->Reset();
	transferMailbox->Reset();
	ownershipElection->Reset(lastTickTime);

	NH_HandoffConnectionManager::Get().Init(selfIdentity, logger);
	NH_HandoffPacketManager::Get().Init(selfIdentity, logger);
	NH_HandoffPacketManager::Get().SetCallbacks(
		[this](const AtlasEntity& entity, const NetworkIdentity& sender,
			   uint64_t transferTick)
		{
			OnIncomingHandoffEntityAtTick(entity, sender, transferTick);
		},
		[](const NetworkIdentity& peer)
		{ NH_HandoffConnectionManager::Get().MarkConnectionActivity(peer); });

	if (logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff][SH] Runtime initialized for {}",
			selfIdentity.ToString());
	}
}

void SH_ServerAuthorityRuntime::Tick()
{
	if (!initialized || !tracker || !debugSimulator || !transferMailbox ||
		!borderPlanner || !telemetryPublisher)
	{
		return;
	}
	++localAuthorityTick;

	NH_HandoffConnectionManager::Get().Tick();
	const auto now = std::chrono::steady_clock::now();
	const float deltaSeconds =
		std::chrono::duration<float>(now - lastTickTime).count();
	lastTickTime = now;
	const bool isOwner =
		ownershipElection && ownershipElection->Evaluate(now);

	const size_t adoptedCount =
		transferMailbox->AdoptIncomingIfDue(localAuthorityTick, *debugSimulator);
	if (adoptedCount > 0 && logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff][SH] Adopted {} incoming handoff entities at tick={}",
			adoptedCount, localAuthorityTick);
	}

	if (isOwner && !hasSeededInitialEntities)
	{
		debugSimulator->SeedEntities(
			DebugEntityOrbitSimulator::SeedOptions{
				.desiredCount = kDefaultTestEntityCount,
				.halfExtent = kTestEntityHalfExtent,
				.phaseStepRad = kEntityPhaseStepRad});
		hasSeededInitialEntities = true;
	}

	if (debugSimulator->Count() > 0)
	{
		debugSimulator->TickOrbit(
			DebugEntityOrbitSimulator::OrbitOptions{
				.deltaSeconds = deltaSeconds,
				.radius = kOrbitRadius,
				.angularSpeedRadPerSec = kOrbitAngularSpeedRadPerSec});
	}
	tracker->SetOwnedEntities(debugSimulator->GetEntitiesSnapshot());

	const auto outgoingHandoffs =
		borderPlanner->PlanAndSendAll(*tracker, localAuthorityTick);
	for (const auto& outgoing : outgoingHandoffs)
	{
		transferMailbox->AddPendingOutgoing(outgoing);
	}

	const size_t committedCount = transferMailbox->CommitOutgoingIfDue(
		localAuthorityTick, *debugSimulator, *tracker, *telemetryPublisher);
	if ((adoptedCount > 0 || committedCount > 0 || !outgoingHandoffs.empty()) &&
		ownershipElection)
	{
		ownershipElection->Invalidate();
	}

	if (!isOwner && tracker->Count() == 0 && !transferMailbox->HasPendingIncoming() &&
		!transferMailbox->HasPendingOutgoing())
	{
		hasSeededInitialEntities = false;
	}

	if (now - lastSnapshotTime >= kStateSnapshotInterval)
	{
		lastSnapshotTime = now;
		telemetryPublisher->PublishFromTracker(*tracker);
	}
}

void SH_ServerAuthorityRuntime::Shutdown()
{
	if (!initialized)
	{
		return;
	}

	NH_HandoffPacketManager::Get().SetCallbacks({}, {});
	NH_HandoffPacketManager::Get().Shutdown();
	NH_HandoffConnectionManager::Get().Shutdown();

	tracker.reset();
	debugSimulator.reset();
	ownershipElection.reset();
	borderPlanner.reset();
	transferMailbox.reset();
	telemetryPublisher.reset();
	initialized = false;

	if (logger)
	{
		logger->Debug("[EntityHandoff][SH] Runtime shutdown");
	}
}

void SH_ServerAuthorityRuntime::OnIncomingHandoffEntity(
	const AtlasEntity& entity, const NetworkIdentity& sender)
{
	OnIncomingHandoffEntityAtTick(entity, sender, 0);
}

void SH_ServerAuthorityRuntime::OnIncomingHandoffEntityAtTick(
	const AtlasEntity& entity, const NetworkIdentity& sender,
	const uint64_t transferTick)
{
	if (!transferMailbox)
	{
		return;
	}

	transferMailbox->QueueIncoming(entity, sender, transferTick);
	if (ownershipElection)
	{
		ownershipElection->Invalidate();
	}
}

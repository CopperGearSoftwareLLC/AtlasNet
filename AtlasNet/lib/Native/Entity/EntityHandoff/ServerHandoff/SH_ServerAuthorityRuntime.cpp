// SH server authority runtime.
// Orchestrates ownership election, simulation, handoff planning, and transfer commit.

#include "SH_ServerAuthorityRuntime.hpp"

#include <chrono>
#include <utility>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityLinearBounceSimulator.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityOrbitSimulator.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntitySimulator.hpp"
#include "SH_BorderHandoffPlanner.hpp"
#include "SH_EntityAuthorityTracker.hpp"
#include "SH_HandoffConnectionManager.hpp"
#include "SH_HandoffPacketManager.hpp"
#include "SH_OwnershipElection.hpp"
#include "SH_TelemetryPublisher.hpp"
#include "SH_TransferMailbox.hpp"

namespace
{
constexpr std::chrono::milliseconds kStateSnapshotInterval(50);
constexpr float kOrbitRadius = 45.0F;
constexpr float kOrbitAngularSpeedRadPerSec = 0.35F;
constexpr float kLinearSpeedUnitsPerSec = 18.0F;
constexpr float kTestEntityHalfExtent = 0.5F;
constexpr float kEntityPhaseStepRad = 0.7F;
constexpr std::chrono::milliseconds kLinearPerimeterRefreshInterval(1000);

#ifndef ATLASNET_ENTITY_HANDOFF_USE_LINEAR_DEBUG_SIM
#define ATLASNET_ENTITY_HANDOFF_USE_LINEAR_DEBUG_SIM 0
#endif

#ifndef ATLASNET_ENTITY_HANDOFF_TRANSFER_DELAY_US
#define ATLASNET_ENTITY_HANDOFF_TRANSFER_DELAY_US 60000
#endif
constexpr int64_t kTransferDelayUsRaw = ATLASNET_ENTITY_HANDOFF_TRANSFER_DELAY_US;
constexpr std::chrono::microseconds kTransferDelay(
	kTransferDelayUsRaw > 0 ? kTransferDelayUsRaw : 0);

uint64_t NowUnixTimeUs()
{
	return static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count());
}

#ifndef ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT
#define ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT 100
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
	lastTickTime = std::chrono::steady_clock::now();
	lastSnapshotTime = lastTickTime - kStateSnapshotInterval;

	tracker = std::make_unique<SH_EntityAuthorityTracker>(selfIdentity, logger);
#if ATLASNET_ENTITY_HANDOFF_USE_LINEAR_DEBUG_SIM
	debugSimulator =
		std::make_unique<DebugEntityLinearBounceSimulator>(selfIdentity, logger);
#else
	debugSimulator =
		std::make_unique<DebugEntityOrbitSimulator>(selfIdentity, logger);
#endif
	ownershipElection =
		std::make_unique<SH_OwnershipElection>(selfIdentity, logger);
	borderPlanner = std::make_unique<SH_BorderHandoffPlanner>(
		selfIdentity, logger,
		SH_BorderHandoffPlanner::Options{.handoffDelay = kTransferDelay});
	transferMailbox = std::make_unique<SH_TransferMailbox>(selfIdentity, logger);
	telemetryPublisher = std::make_unique<SH_TelemetryPublisher>();

	tracker->Reset();
	debugSimulator->Reset();
	transferMailbox->Reset();
	ownershipElection->Reset(lastTickTime);

	SH_HandoffConnectionManager::Get().Init(selfIdentity, logger);
	SH_HandoffPacketManager::Get().Init(selfIdentity, logger);
	SH_HandoffPacketManager::Get().SetCallbacks(
		[this](const AtlasEntity& entity, const NetworkIdentity& sender,
			   uint64_t transferTimeUs)
		{
			OnIncomingHandoffEntityAtTimeUs(entity, sender, transferTimeUs);
		},
		[](const NetworkIdentity& peer)
		{ SH_HandoffConnectionManager::Get().MarkConnectionActivity(peer); });

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

	SH_HandoffConnectionManager::Get().Tick();
	const auto now = std::chrono::steady_clock::now();
	const uint64_t nowUnixTimeUs = NowUnixTimeUs();
	const float deltaSeconds =
		std::chrono::duration<float>(now - lastTickTime).count();
	lastTickTime = now;
	const bool isOwner =
		ownershipElection && ownershipElection->Evaluate(now);

	const size_t adoptedCount =
		transferMailbox->AdoptIncomingIfDue(nowUnixTimeUs, *debugSimulator);
	if (adoptedCount > 0 && logger)
	{
		logger->DebugFormatted(
			"[EntityHandoff][SH] Adopted {} incoming handoff entities at "
			"now_unix_us={}",
			adoptedCount, nowUnixTimeUs);
	}

	if (isOwner && !hasSeededInitialEntities)
	{
		debugSimulator->SeedEntities(
			DebugEntitySimulator::SeedOptions{
				.desiredCount = kDefaultTestEntityCount,
				.halfExtent = kTestEntityHalfExtent,
				.phaseStepRad = kEntityPhaseStepRad,
				.speedUnitsPerSec = kLinearSpeedUnitsPerSec});
		hasSeededInitialEntities = true;
	}

	if (debugSimulator->Count() > 0)
	{
		debugSimulator->Tick(
			DebugEntitySimulator::TickOptions{
				.deltaSeconds = deltaSeconds,
				.radius = kOrbitRadius,
				.angularSpeedRadPerSec = kOrbitAngularSpeedRadPerSec,
				.perimeterRefreshInterval = kLinearPerimeterRefreshInterval});
	}
	tracker->SetOwnedEntities(debugSimulator->GetEntitiesSnapshot());

	const auto outgoingHandoffs =
		borderPlanner->PlanAndSendAll(*tracker, nowUnixTimeUs);
	for (const auto& outgoing : outgoingHandoffs)
	{
		transferMailbox->AddPendingOutgoing(outgoing);
	}

	const size_t committedCount = transferMailbox->CommitOutgoingIfDue(
		nowUnixTimeUs, *debugSimulator, *tracker, *telemetryPublisher);
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

	SH_HandoffPacketManager::Get().SetCallbacks({}, {});
	SH_HandoffPacketManager::Get().Shutdown();
	SH_HandoffConnectionManager::Get().Shutdown();

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
	OnIncomingHandoffEntityAtTimeUs(entity, sender, 0);
}

void SH_ServerAuthorityRuntime::OnIncomingHandoffEntityAtTimeUs(
	const AtlasEntity& entity, const NetworkIdentity& sender,
	const uint64_t transferTimeUs)
{
	if (!transferMailbox)
	{
		return;
	}

	transferMailbox->QueueIncoming(entity, sender, transferTimeUs);
	if (ownershipElection)
	{
		ownershipElection->Invalidate();
	}
}

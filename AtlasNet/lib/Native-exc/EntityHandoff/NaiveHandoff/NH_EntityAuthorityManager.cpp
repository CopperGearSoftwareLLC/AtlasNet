// NH naive handoff coordinator runtime.
// Responsibilities: ownership election, simulator tick, boundary-triggered
// handoff send, incoming adoption, and transfer-tick commit.

#include "NH_EntityAuthorityManager.hpp"

#include <chrono>
#include <optional>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntityOrbitSimulator.hpp"
#include "NH_HandoffConnectionManager.hpp"
#include "NH_EntityAuthorityTracker.hpp"
#include "NH_HandoffPacketManager.hpp"
#include "Entity/EntityHandoff/Telemetry/AuthorityManifest.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
constexpr std::chrono::seconds kOwnershipEvalInterval(2);
constexpr std::chrono::milliseconds kStateSnapshotInterval(250);
constexpr float kOrbitRadius = 50.0F;
constexpr float kOrbitAngularSpeedRadPerSec = 0.35F;
constexpr float kTestEntityHalfExtent = 0.5F;
constexpr float kEntityPhaseStepRad = 0.7F;
constexpr uint64_t kHandoffLeadTicks = 6;

#ifndef ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT
#define ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT 1
#endif
constexpr uint32_t kDefaultTestEntityCount =
	(ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT > 0)
		? static_cast<uint32_t>(ATLASNET_ENTITY_HANDOFF_TEST_ENTITY_COUNT)
		: 1U;

constexpr std::string_view kTestOwnerKey = "EntityHandoff:TestOwnerShard";

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

std::vector<AuthorityManifest::TelemetryRow> ToManifestRows(
	const std::vector<NH_EntityAuthorityTracker::AuthorityTelemetryRow>& trackerRows)
{
	std::vector<AuthorityManifest::TelemetryRow> rows;
	rows.reserve(trackerRows.size());
	for (const auto& trackerRow : trackerRows)
	{
		AuthorityManifest::TelemetryRow row;
		row.entityId = trackerRow.entityId;
		row.owner = trackerRow.owner;
		row.entitySnapshot = trackerRow.entitySnapshot;
		row.world = trackerRow.world;
		row.position = trackerRow.position;
		row.isClient = trackerRow.isClient;
		row.clientId = trackerRow.clientId;
		rows.push_back(std::move(row));
	}
	return rows;
}
}  // namespace

class NH_EntityAuthorityManager::Runtime
{
  public:
	Runtime() = default;
	~Runtime()
	{
		if (initialized)
		{
			Shutdown();
		}
	}

	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger)
	{
		selfIdentity = self;
		logger = std::move(inLogger);
		initialized = true;
		isTestEntityOwner = false;
		ownershipEvaluated = false;
		hasOwnershipLogState = false;
		lastOwnershipState = false;
		tracker = std::make_unique<NH_EntityAuthorityTracker>(selfIdentity, logger);
		debugSimulator =
			std::make_unique<DebugEntityOrbitSimulator>(selfIdentity, logger);
		tracker->Reset();
		debugSimulator->Reset();
		pendingIncomingEntity.reset();
		pendingOutgoingHandoff.reset();
		localAuthorityTick = 0;
		lastTickTime = std::chrono::steady_clock::now();
		lastOwnerEvalTime = lastTickTime - kOwnershipEvalInterval;
		lastSnapshotTime = lastTickTime - kStateSnapshotInterval;

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
				"[EntityHandoff] NH_EntityAuthorityManager runtime initialized for {}",
				selfIdentity.ToString());
		}
	}

	void Tick()
	{
		if (!initialized)
		{
			return;
		}
		++localAuthorityTick;

		NH_HandoffConnectionManager::Get().Tick();
		EvaluateTestEntityOwnership();
		if (isTestEntityOwner && tracker && debugSimulator)
		{
			if (pendingIncomingEntity.has_value() &&
				localAuthorityTick >= pendingIncomingEntity->transferTick)
			{
				const AtlasEntity adopted = pendingIncomingEntity->entity;
				debugSimulator->AdoptSingleEntity(adopted);
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
			ProcessOutgoingHandoffCommit();
			if (now - lastSnapshotTime >= kStateSnapshotInterval)
			{
				lastSnapshotTime = now;
				std::vector<NH_EntityAuthorityTracker::AuthorityTelemetryRow> trackerRows;
				tracker->CollectTelemetryRows(trackerRows);
				AuthorityManifest::Get().TelemetryUpdate(ToManifestRows(trackerRows));
			}
		}
		else if (tracker && debugSimulator)
		{
			tracker->SetOwnedEntities({});
			std::vector<NH_EntityAuthorityTracker::AuthorityTelemetryRow> trackerRows;
			tracker->CollectTelemetryRows(trackerRows);
			AuthorityManifest::Get().TelemetryUpdate(ToManifestRows(trackerRows));
			debugSimulator->Reset();
			pendingOutgoingHandoff.reset();
		}
	}

	void Shutdown()
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
		pendingIncomingEntity.reset();
		pendingOutgoingHandoff.reset();
		initialized = false;

		if (logger)
		{
			logger->Debug("[EntityHandoff] NH_EntityAuthorityManager runtime shutdown");
		}
	}

	void OnIncomingHandoffEntity(const AtlasEntity& entity,
							 const NetworkIdentity& sender)
	{
		OnIncomingHandoffEntityAtTick(entity, sender, 0);
	}

	void OnIncomingHandoffEntityAtTick(const AtlasEntity& entity,
								   const NetworkIdentity& sender,
								   uint64_t transferTick)
	{
		pendingIncomingEntity = NH_EntityAuthorityManager::PendingIncomingHandoff{
			.entity = entity, .sender = sender, .transferTick = transferTick};
		ownershipEvaluated = false;
		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff] Received handoff entity={} from {} transfer_tick={}",
				entity.Entity_ID, sender.ToString(), transferTick);
		}
	}

	[[nodiscard]] bool IsInitialized() const { return initialized; }

  private:
	void EvaluateHeuristicPositionTriggers()
	{
		if (!tracker)
		{
			return;
		}

		std::unordered_map<NetworkIdentity, GridShape> claimedBounds;
		HeuristicManifest::Get().GetAllClaimedBounds<GridShape>(
			claimedBounds);
		if (claimedBounds.empty())
		{
			return;
		}

		const std::string selfKey = selfIdentity.ToString();
		const auto selfIt = claimedBounds.find(selfIdentity);
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

			const NetworkIdentity targetClaimKey = SelectTargetClaimKeyForPosition(
				claimedBounds, selfIdentity, position);

			const auto targetIdentity = ResolveIdentityFromClaimKey(
				targetClaimKey, ServerRegistry::Get().GetServers());
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
			const uint64_t transferTick = localAuthorityTick + kHandoffLeadTicks;
			NH_HandoffPacketManager::Get().SendEntityHandoff(*targetIdentity, entity,
													  transferTick);
			const bool ownerSwitched =
				InternalDB::Get()->Set(kTestOwnerKey, targetClaimKey.ToString());
			(void)ownerSwitched;
			pendingOutgoingHandoff = NH_EntityAuthorityManager::PendingOutgoingHandoff{
				.entityId = entity.Entity_ID,
				.targetIdentity = *targetIdentity,
				.transferTick = transferTick};
			ownershipEvaluated = false;

			if (logger)
			{
				logger->WarningFormatted(
					"[EntityHandoff] Triggered passing state entity={} pos={} "
					"self_bound_id={} target_claim={} target_id={} transfer_tick={}",
					entity.Entity_ID, glm::to_string(position), selfBounds.GetID(),
					targetClaimKey.ToString(), targetIdentity->ToString(), transferTick);
			}
		}
	}

	void EvaluateTestEntityOwnership()  // NOLINT(readability-function-cognitive-complexity)
	{
		const auto now = std::chrono::steady_clock::now();
		if (ownershipEvaluated &&
			now - lastOwnerEvalTime < kOwnershipEvalInterval)
		{
			return;
		}
		lastOwnerEvalTime = now;
		ownershipEvaluated = true;

		const auto& servers = ServerRegistry::Get().GetServers();
		std::optional<std::string> selectedOwner = InternalDB::Get()->Get(kTestOwnerKey);
		if (!selectedOwner.has_value() || selectedOwner->empty())
		{
			selectedOwner = SelectBootstrapOwner(servers);
			if (selectedOwner.has_value())
			{
				const bool ownerWritten =
					InternalDB::Get()->Set(kTestOwnerKey, selectedOwner.value());
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
						InternalDB::Get()->Set(kTestOwnerKey, selectedOwner.value());
					(void)ownerWritten;
				}
			}
		}

		if (!selectedOwner.has_value())
		{
			isTestEntityOwner = false;
			return;
		}

		isTestEntityOwner = (selectedOwner.value() == selfIdentity.ToString());

		if (logger &&
			(!hasOwnershipLogState || lastOwnershipState != isTestEntityOwner))
		{
			hasOwnershipLogState = true;
			lastOwnershipState = isTestEntityOwner;
			logger->DebugFormatted(
				"[EntityHandoff] Ownership transition owner={} self={} owning={}",
				selectedOwner.value(), selfIdentity.ToString(),
				isTestEntityOwner ? "yes" : "no");
		}
	}

	void ProcessOutgoingHandoffCommit()
	{
		if (!pendingOutgoingHandoff.has_value() || !debugSimulator || !tracker)
		{
			return;
		}
		if (localAuthorityTick < pendingOutgoingHandoff->transferTick)
		{
			return;
		}

		debugSimulator->Reset();
		tracker->SetOwnedEntities({});
		std::vector<NH_EntityAuthorityTracker::AuthorityTelemetryRow> trackerRows;
		tracker->CollectTelemetryRows(trackerRows);
		AuthorityManifest::Get().TelemetryUpdate(ToManifestRows(trackerRows));
		isTestEntityOwner = false;
		ownershipEvaluated = false;

		if (logger)
		{
			logger->WarningFormatted(
				"[EntityHandoff] Committed outgoing handoff entity={} target={} "
				"transfer_tick={}",
				pendingOutgoingHandoff->entityId,
				pendingOutgoingHandoff->targetIdentity.ToString(),
				pendingOutgoingHandoff->transferTick);
		}

		pendingOutgoingHandoff.reset();
	}

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	bool initialized = false;
	bool isTestEntityOwner = false;
	bool ownershipEvaluated = false;
	bool hasOwnershipLogState = false;
	bool lastOwnershipState = false;
	std::unique_ptr<NH_EntityAuthorityTracker> tracker;
	std::unique_ptr<DebugEntityOrbitSimulator> debugSimulator;
	std::optional<NH_EntityAuthorityManager::PendingIncomingHandoff> pendingIncomingEntity;
	std::optional<NH_EntityAuthorityManager::PendingOutgoingHandoff>
		pendingOutgoingHandoff;
	uint64_t localAuthorityTick = 0;
	std::chrono::steady_clock::time_point lastTickTime;
	std::chrono::steady_clock::time_point lastOwnerEvalTime;
	std::chrono::steady_clock::time_point lastSnapshotTime;
};

NH_EntityAuthorityManager::NH_EntityAuthorityManager() = default;
NH_EntityAuthorityManager::~NH_EntityAuthorityManager() = default;

void NH_EntityAuthorityManager::Init(const NetworkIdentity& self,
								  std::shared_ptr<Log> inLogger)
{
	if (!runtime)
	{
		runtime = std::make_unique<Runtime>();
	}
	runtime->Init(self, std::move(inLogger));
}

void NH_EntityAuthorityManager::Tick()
{
	if (!runtime)
	{
		return;
	}
	runtime->Tick();
}

void NH_EntityAuthorityManager::Shutdown()
{
	if (!runtime)
	{
		return;
	}
	runtime->Shutdown();
}

void NH_EntityAuthorityManager::OnIncomingHandoffEntity(
	const AtlasEntity& entity, const NetworkIdentity& sender)
{
	if (!runtime)
	{
		return;
	}
	runtime->OnIncomingHandoffEntity(entity, sender);
}

void NH_EntityAuthorityManager::OnIncomingHandoffEntityAtTick(
	const AtlasEntity& entity, const NetworkIdentity& sender,
	const uint64_t transferTick)
{
	if (!runtime)
	{
		return;
	}
	runtime->OnIncomingHandoffEntityAtTick(entity, sender, transferTick);
}

bool NH_EntityAuthorityManager::IsInitialized() const
{
	return runtime && runtime->IsInitialized();
}

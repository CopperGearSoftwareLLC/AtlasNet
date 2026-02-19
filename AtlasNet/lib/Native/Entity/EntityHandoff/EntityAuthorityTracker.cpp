// Implements in-memory entity authority state tracking and telemetry row export.

#include "Entity/EntityHandoff/EntityAuthorityTracker.hpp"

#include <unordered_set>

EntityAuthorityTracker::EntityAuthorityTracker(const NetworkIdentity& self,
											   std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void EntityAuthorityTracker::Reset()
{
	authorityByEntityId.clear();
}

void EntityAuthorityTracker::SetOwnedEntities(
	const std::vector<AtlasEntity>& ownedEntities)
{
	std::unordered_set<AtlasEntityID> keepIds;
	keepIds.reserve(ownedEntities.size());

	for (const auto& entity : ownedEntities)
	{
		keepIds.insert(entity.Entity_ID);
		auto it = authorityByEntityId.find(entity.Entity_ID);
		if (it == authorityByEntityId.end())
		{
			AuthorityEntry entry;
			entry.entitySnapshot = entity;
			entry.authorityState = AuthorityState::eAuthoritative;
			authorityByEntityId.emplace(entity.Entity_ID, std::move(entry));
		}
		else
		{
			it->second.entitySnapshot = entity;
		}
	}

	std::vector<AtlasEntityID> toRemove;
	toRemove.reserve(authorityByEntityId.size());
	for (const auto& [entityId, _entry] : authorityByEntityId)
	{
		if (!keepIds.contains(entityId))
		{
			toRemove.push_back(entityId);
		}
	}
	for (const auto entityId : toRemove)
	{
		authorityByEntityId.erase(entityId);
	}
}

void EntityAuthorityTracker::CollectTelemetryRows(
	std::vector<AuthorityTelemetryRow>& outRows) const
{
	outRows.clear();
	outRows.reserve(authorityByEntityId.size());
	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		AuthorityTelemetryRow row;
		row.entityId = entityId;
		row.owner = selfIdentity;
		row.entitySnapshot = entry.entitySnapshot;
		row.world = entry.entitySnapshot.transform.world;
		row.position = entry.entitySnapshot.transform.position;
		row.isClient = entry.entitySnapshot.IsClient;
		row.clientId = entry.entitySnapshot.Client_ID;
		outRows.push_back(std::move(row));
	}
}

void EntityAuthorityTracker::DebugLogTrackedEntities() const
{
	if (!logger)
	{
		return;
	}

	logger->DebugFormatted("[EntityHandoff] Tracker entities={}",
						   authorityByEntityId.size());
	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		const char* stateLabel =
			entry.authorityState == AuthorityState::eAuthoritative
				? "authoritative"
				: "passing";
		logger->DebugFormatted(
			"[EntityHandoff] entity={} state={} pos={} owner={}", entityId,
			stateLabel, glm::to_string(entry.entitySnapshot.transform.position),
			selfIdentity.ToString());
	}
}

std::vector<AtlasEntity> EntityAuthorityTracker::GetOwnedEntitySnapshots() const
{
	std::vector<AtlasEntity> snapshots;
	snapshots.reserve(authorityByEntityId.size());
	for (const auto& [_entityId, entry] : authorityByEntityId)
	{
		snapshots.push_back(entry.entitySnapshot);
	}
	return snapshots;
}

bool EntityAuthorityTracker::MarkPassing(AtlasEntityID entityId,
										 const NetworkIdentity& passingTarget)
{
	const auto it = authorityByEntityId.find(entityId);
	if (it == authorityByEntityId.end())
	{
		return false;
	}

	if (it->second.authorityState == AuthorityState::ePassing &&
		it->second.passingTo.has_value() &&
		it->second.passingTo.value() == passingTarget)
	{
		return false;
	}

	it->second.authorityState = AuthorityState::ePassing;
	it->second.passingTo = passingTarget;
	return true;
}

void EntityAuthorityTracker::MarkAuthoritative(AtlasEntityID entityId)
{
	const auto it = authorityByEntityId.find(entityId);
	if (it == authorityByEntityId.end())
	{
		return;
	}
	it->second.authorityState = AuthorityState::eAuthoritative;
	it->second.passingTo.reset();
}

void EntityAuthorityTracker::SetAuthorityState(
	AtlasEntityID entityId, const AuthorityState state,
	const std::optional<NetworkIdentity>& passingTo)
{
	const auto it = authorityByEntityId.find(entityId);
	if (it == authorityByEntityId.end())
	{
		return;
	}
	it->second.authorityState = state;
	it->second.passingTo = passingTo;
}

void EntityAuthorityTracker::RemoveEntity(const AtlasEntityID entityId)
{
	authorityByEntityId.erase(entityId);
}

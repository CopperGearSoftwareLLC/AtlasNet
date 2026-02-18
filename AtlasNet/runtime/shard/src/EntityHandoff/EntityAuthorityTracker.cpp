#include "EntityHandoff/EntityAuthorityTracker.hpp"

#include <format>
#include <unordered_set>

#include "InternalDB.hpp"

EntityAuthorityTracker::EntityAuthorityTracker(const NetworkIdentity& self,
											   std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void EntityAuthorityTracker::Reset()
{
	authorityByEntityId.clear();
	lastPublishedFields.clear();
}

void EntityAuthorityTracker::SetOwnedEntities(
	const std::vector<AtlasEntity>& ownedEntities)
{
	std::unordered_set<AtlasEntity::EntityID> incoming;
	incoming.reserve(ownedEntities.size());
	for (const auto& entity : ownedEntities)
	{
		incoming.insert(entity.Entity_ID);
	}
	for (auto entityIt = authorityByEntityId.begin();
		 entityIt != authorityByEntityId.end();)
	{
		if (incoming.contains(entityIt->first))
		{
			++entityIt;
			continue;
		}
		entityIt = authorityByEntityId.erase(entityIt);
	}

	for (const auto& entity : ownedEntities)
	{
		auto& entry = authorityByEntityId[entity.Entity_ID];
		entry.entitySnapshot = entity;
		if (!entry.passingTo.has_value() &&
			entry.authorityState != AuthorityState::ePassing)
		{
			entry.authorityState = AuthorityState::eAuthoritative;
		}
	}
}

void EntityAuthorityTracker::StoreAuthorityStateSnapshots(std::string_view hashKey)
{
	std::unordered_set<std::string> nextPublishedFields;
	nextPublishedFields.reserve(authorityByEntityId.size());

	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		const std::string field =
			std::format("{}|{}", selfIdentity.ToString(), entityId);
		const char* stateString =
			entry.authorityState == AuthorityState::eAuthoritative
				? "authoritative"
				: "passing";
		const std::string passingTo =
			entry.passingTo.has_value() ? entry.passingTo->ToString() : "";
		const std::string value = std::format("{}\t{}", stateString, passingTo);
		const long long wrote = InternalDB::Get()->HSet(hashKey, field, value);
		(void)wrote;
		nextPublishedFields.insert(field);
	}

	std::vector<std::string> staleFields;
	for (const auto& oldField : lastPublishedFields)
	{
		if (!nextPublishedFields.contains(oldField))
		{
			staleFields.push_back(oldField);
		}
	}

	if (!staleFields.empty())
	{
		std::vector<std::string_view> staleFieldViews;
		staleFieldViews.reserve(staleFields.size());
		for (const auto& field : staleFields)
		{
			staleFieldViews.push_back(field);
		}
		const long long removed = InternalDB::Get()->HDel(hashKey, staleFieldViews);
		(void)removed;
	}

	lastPublishedFields = std::move(nextPublishedFields);
}

void EntityAuthorityTracker::SetAuthorityState(
	AtlasEntity::EntityID entityId, AuthorityState state,
	const std::optional<NetworkIdentity>& passingTo)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return;
	}

	entityIt->second.authorityState = state;
	entityIt->second.passingTo = passingTo;
}

void EntityAuthorityTracker::RemoveEntity(AtlasEntity::EntityID entityId)
{
	authorityByEntityId.erase(entityId);
}

bool EntityAuthorityTracker::MarkPassing(
	AtlasEntity::EntityID entityId, const NetworkIdentity& passingTarget)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return false;
	}

	const bool alreadyPassingToTarget =
		entityIt->second.authorityState == AuthorityState::ePassing &&
		entityIt->second.passingTo.has_value() &&
		entityIt->second.passingTo.value() == passingTarget;
	entityIt->second.authorityState = AuthorityState::ePassing;
	entityIt->second.passingTo = passingTarget;
	return !alreadyPassingToTarget;
}

void EntityAuthorityTracker::MarkAuthoritative(AtlasEntity::EntityID entityId)
{
	auto entityIt = authorityByEntityId.find(entityId);
	if (entityIt == authorityByEntityId.end())
	{
		return;
	}

	entityIt->second.authorityState = AuthorityState::eAuthoritative;
	entityIt->second.passingTo.reset();
}

void EntityAuthorityTracker::DebugLogTrackedEntities() const
{
	if (!logger)
	{
		return;
	}

	if (authorityByEntityId.size() > 0)
	{
		logger->DebugFormatted("[EntityHandoff] AuthorityTracker owns {} entities", authorityByEntityId.size());
	}


	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		const char* state =
			entry.authorityState == AuthorityState::eAuthoritative
				? "authoritative"
				: "passing";
		const std::string passingTo =
			entry.passingTo.has_value() ? entry.passingTo->ToString() : "-";
		const auto& pos = entry.entitySnapshot.transform.position;
		logger->DebugFormatted(
			"[EntityHandoff]  entity={} pos=({}, {}, {}) state={} passing_to={} world={}",
			entityId, pos[0], pos[1], pos[2], state, passingTo, entry.entitySnapshot.transform.world);
	}
}

std::vector<AtlasEntity> EntityAuthorityTracker::GetOwnedEntitySnapshots() const
{
	std::vector<AtlasEntity> entities;
	entities.reserve(authorityByEntityId.size());
	for (const auto& [entityId, entry] : authorityByEntityId)
	{
		(void)entityId;
		entities.push_back(entry.entitySnapshot);
	}
	return entities;
}

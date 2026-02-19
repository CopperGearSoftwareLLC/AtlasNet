// Implements deterministic debug entity spawning, orbit updates, and snapshot writes.

#include "Entity/EntityHandoff/DebugEntityOrbitSimulator.hpp"

#include <cmath>
#include "Entity/Entity.hpp"

namespace
{
AtlasEntityID MakeEntityId(const NetworkIdentity& self, uint32_t index)
{
	const auto base = static_cast<AtlasEntityID>(
		std::hash<std::string>{}(self.ToString()));
	return base ^ (static_cast<AtlasEntityID>(index + 1U) << 1U);
}
}  // namespace

DebugEntityOrbitSimulator::DebugEntityOrbitSimulator(const NetworkIdentity& self,
													 std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void DebugEntityOrbitSimulator::Reset()
{
	entitiesById.clear();
	orbitAngleRad = 0.0F;
}

void DebugEntityOrbitSimulator::SeedEntities(const SeedOptions& options)
{
	if (entitiesById.size() >= options.desiredCount)
	{
		return;
	}

	for (uint32_t i = static_cast<uint32_t>(entitiesById.size());
		 i < options.desiredCount; ++i)
	{
		AtlasEntity entity;
		entity.Entity_ID = MakeEntityId(selfIdentity, i);
		entity.transform.world = 0;
		entity.transform.position = vec3(0.0F, 0.0F, 0.0F);
		entity.transform.boundingBox.SetCenterExtents(
			entity.transform.position,
			vec3(options.halfExtent, options.halfExtent, options.halfExtent));
		entity.IsClient = false;
		entity.Client_ID = UUID();
		entity.Metadata.clear();

		OrbitEntity orbitEntity;
		orbitEntity.entity = entity;
		orbitEntity.phaseOffsetRad = options.phaseStepRad * static_cast<float>(i);
		entitiesById[entity.Entity_ID] = orbitEntity;
	}
}

void DebugEntityOrbitSimulator::AdoptSingleEntity(const AtlasEntity& entity)
{
	OrbitEntity orbitEntity;
	orbitEntity.entity = entity;
	orbitEntity.phaseOffsetRad = 0.0F;
	entitiesById[entity.Entity_ID] = orbitEntity;
}

void DebugEntityOrbitSimulator::TickOrbit(const OrbitOptions& options)
{
	orbitAngleRad += options.deltaSeconds * options.angularSpeedRadPerSec;
	for (auto& [entityId, orbitEntity] : entitiesById)
	{
		(void)entityId;
		const float angle = orbitAngleRad + orbitEntity.phaseOffsetRad;
		const vec3 position =
			vec3(std::cos(angle) * options.radius, std::sin(angle) * options.radius, 0.0F);
		orbitEntity.entity.transform.position = position;
		orbitEntity.entity.transform.boundingBox.SetCenterExtents(
			position, vec3(kDefaultHalfExtent, kDefaultHalfExtent, kDefaultHalfExtent));
	}
}

void DebugEntityOrbitSimulator::StoreStateSnapshots(std::string_view hashKey) const
{
	(void)hashKey;
}

std::vector<AtlasEntity> DebugEntityOrbitSimulator::GetEntitiesSnapshot() const
{
	std::vector<AtlasEntity> snapshots;
	snapshots.reserve(entitiesById.size());
	for (const auto& [_entityId, orbitEntity] : entitiesById)
	{
		snapshots.push_back(orbitEntity.entity);
	}
	return snapshots;
}

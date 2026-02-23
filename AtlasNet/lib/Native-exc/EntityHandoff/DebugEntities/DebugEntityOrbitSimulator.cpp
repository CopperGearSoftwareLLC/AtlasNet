// Implements deterministic debug entity spawning, orbit updates, and snapshot writes.

#include "DebugEntityOrbitSimulator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include "Entity/Entity.hpp"

namespace
{
AtlasEntityID MakeEntityId(uint32_t index)
{
	// Keep debug entity IDs shard-agnostic so ownership transfer does not create
	// a second synthetic entity with a different ID on the receiving shard.
	static constexpr AtlasEntityID kDebugEntityIdNamespace =
		static_cast<AtlasEntityID>(0xA7105EED00000000ULL);
	return kDebugEntityIdNamespace ^
		   (static_cast<AtlasEntityID>(index + 1U) << 1U);
}
}  // namespace

DebugEntityOrbitSimulator::DebugEntityOrbitSimulator(const NetworkIdentity& self,
													 std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
	std::random_device rd;
	rng.seed((static_cast<uint64_t>(rd()) << 32U) ^ static_cast<uint64_t>(rd()));
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
		const float phaseOffset = [&]() -> float
		{
			if (!options.randomizeInitialPhase)
			{
				return options.phaseStepRad * static_cast<float>(i);
			}
			std::uniform_real_distribution<float> phaseDist(
				0.0F, static_cast<float>(2.0) * std::numbers::pi_v<float>);
			return phaseDist(rng);
		}();
		const float angle = orbitAngleRad + phaseOffset;
		const float radius = std::max(options.initialRadius, 0.0F);
		const vec3 initialPosition =
			vec3(std::cos(angle) * radius, std::sin(angle) * radius, 0.0F);

		AtlasEntity entity;
		entity.Entity_ID = MakeEntityId(i);
		entity.transform.world = 0;
		entity.transform.position = initialPosition;
		entity.transform.boundingBox.SetCenterExtents(
			entity.transform.position,
			vec3(options.halfExtent, options.halfExtent, options.halfExtent));
		entity.IsClient = false;
		entity.Client_ID = UUID();
		entity.Metadata.clear();

		OrbitEntity orbitEntity;
		orbitEntity.entity = entity;
		orbitEntity.phaseOffsetRad = phaseOffset;
		entitiesById[entity.Entity_ID] = orbitEntity;
	}
}

void DebugEntityOrbitSimulator::AdoptSingleEntity(const AtlasEntity& entity)
{
	OrbitEntity orbitEntity;
	orbitEntity.entity = entity;
	const vec3 position = entity.transform.position;
	if (std::abs(position.x) > 1e-4F || std::abs(position.y) > 1e-4F)
	{
		const float angleRad = std::atan2(position.y, position.x);
		orbitEntity.phaseOffsetRad = angleRad - orbitAngleRad;
	}
	else
	{
		orbitEntity.phaseOffsetRad = 0.0F;
	}
	entitiesById[entity.Entity_ID] = orbitEntity;
}

void DebugEntityOrbitSimulator::RemoveEntity(const AtlasEntityID entityId)
{
	entitiesById.erase(entityId);
}

void DebugEntityOrbitSimulator::Tick(const TickOptions& options)
{
	const float deltaSeconds = std::clamp(options.deltaSeconds, 0.0F, 0.25F);
	orbitAngleRad += deltaSeconds * options.angularSpeedRadPerSec;
	for (auto& [entityId, orbitEntity] : entitiesById)
	{
		(void)entityId;
		const float angle = orbitAngleRad + orbitEntity.phaseOffsetRad;
		const vec3 position = vec3(std::cos(angle) * options.radius,
								   std::sin(angle) * options.radius,
								   orbitEntity.entity.transform.position.z);
		orbitEntity.entity.transform.position = position;
		orbitEntity.entity.transform.boundingBox.SetCenterExtents(
			position,
			vec3(kDefaultHalfExtent, kDefaultHalfExtent, kDefaultHalfExtent));
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

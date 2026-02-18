#include "EntityHandoff/DebugEntityOrbitSimulator.hpp"

#include <cmath>
#include <format>

#include "InternalDB.hpp"
#include "Serialize/ByteWriter.hpp"

namespace
{
constexpr std::string_view kTestStatsHash = "EntityHandoff:TestStats";
constexpr std::string_view kTestStatsFieldCreated = "created_entities";
}

DebugEntityOrbitSimulator::DebugEntityOrbitSimulator(
	const NetworkIdentity& self, std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
}

void DebugEntityOrbitSimulator::Reset()
{
	orbitAngleRad = 0.0F;
	entitiesById.clear();
}

void DebugEntityOrbitSimulator::SeedEntities(const SeedOptions& options)
{
	if (entitiesById.size() >= options.desiredCount)
	{
		return;
	}

	const auto shardSeed =
		static_cast<AtlasEntity::EntityID>(std::hash<std::string>{}(
			selfIdentity.ToString()));
	for (uint32_t entityIndex = static_cast<uint32_t>(entitiesById.size());
		 entityIndex < options.desiredCount; ++entityIndex)
	{
		OrbitEntity tracked;
		tracked.entity.Entity_ID = shardSeed ^ static_cast<uint64_t>(entityIndex + 1);
		tracked.entity.transform.world = 0;
		tracked.entity.transform.position = vec3(0.0F, 0.0F, 0.0F);
		tracked.entity.transform.boundingBox =
			AABB3f(vec3(-options.halfExtent), vec3(options.halfExtent));
		tracked.entity.IsClient = false;
		tracked.entity.Client_ID = 0;
		tracked.entity.Metadata.clear();
		tracked.phaseOffsetRad =
			static_cast<float>(entityIndex) * options.phaseStepRad;
		entitiesById.emplace(tracked.entity.Entity_ID, std::move(tracked));

		const long long createdCount =
			InternalDB::Get()->HIncrBy(kTestStatsHash, kTestStatsFieldCreated, 1);
		(void)createdCount;
	}
}

void DebugEntityOrbitSimulator::TickOrbit(const OrbitOptions& options)
{
	orbitAngleRad += options.deltaSeconds * options.angularSpeedRadPerSec;
	for (auto& [entityId, tracked] : entitiesById)
	{
		(void)entityId;
		const float angle = orbitAngleRad + tracked.phaseOffsetRad;
		tracked.entity.transform.position =
			vec3(options.radius * std::cos(angle), options.radius * std::sin(angle),
				 0.0F);
	}
}

void DebugEntityOrbitSimulator::StoreStateSnapshots(std::string_view hashKey) const
{
	for (const auto& [entityId, tracked] : entitiesById)
	{
		ByteWriter stateWriter;
		tracked.entity.Serialize(stateWriter);
		const std::string field =
			std::format("{}|{}", selfIdentity.ToString(), entityId);
		const std::string value = std::string(stateWriter.as_string_view());
		const long long wrote = InternalDB::Get()->HSet(hashKey, field, value);
		(void)wrote;
	}
}

std::vector<AtlasEntity> DebugEntityOrbitSimulator::GetEntitiesSnapshot() const
{
	std::vector<AtlasEntity> entities;
	entities.reserve(entitiesById.size());
	for (const auto& [_entityId, tracked] : entitiesById)
	{
		(void)_entityId;
		entities.push_back(tracked.entity);
	}
	return entities;
}

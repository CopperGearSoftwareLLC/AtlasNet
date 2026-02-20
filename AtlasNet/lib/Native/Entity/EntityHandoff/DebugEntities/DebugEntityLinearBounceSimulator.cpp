// Implements deterministic-ish debug entity spawning and linear bounce
// simulation against the combined world perimeter bounds.

#include "DebugEntityLinearBounceSimulator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/GridHeuristic/GridHeuristic.hpp"
#include "Network/NetworkIdentity.hpp"

namespace
{
AtlasEntityID MakeEntityId(uint32_t index)
{
	static constexpr AtlasEntityID kDebugEntityIdNamespace =
		static_cast<AtlasEntityID>(0xB01A7EED00000000ULL);
	return kDebugEntityIdNamespace ^
		   (static_cast<AtlasEntityID>(index + 1U) << 1U);
}

void ReflectAxis(float& position, float& velocity, float minValue, float maxValue)
{
	if (minValue > maxValue)
	{
		const float midpoint = (minValue + maxValue) * 0.5F;
		position = midpoint;
		velocity = 0.0F;
		return;
	}

	for (int bounceCount = 0; bounceCount < 4; ++bounceCount)
	{
		if (position < minValue)
		{
			position = minValue + (minValue - position);
			velocity = std::abs(velocity);
			continue;
		}
		if (position > maxValue)
		{
			position = maxValue - (position - maxValue);
			velocity = -std::abs(velocity);
			continue;
		}
		break;
	}

	position = std::clamp(position, minValue, maxValue);
}
}  // namespace

DebugEntityLinearBounceSimulator::DebugEntityLinearBounceSimulator(
	const NetworkIdentity& self, std::shared_ptr<Log> inLogger)
	: selfIdentity(self), logger(std::move(inLogger))
{
	std::random_device rd;
	rng.seed((static_cast<uint64_t>(rd()) << 32U) ^ static_cast<uint64_t>(rd()));
}

void DebugEntityLinearBounceSimulator::Reset()
{
	entitiesById.clear();
	worldPerimeterValid = false;
	lastPerimeterRefresh = std::chrono::steady_clock::time_point::min();
}

void DebugEntityLinearBounceSimulator::SeedEntities(const SeedOptions& options)
{
	if (entitiesById.size() >= options.desiredCount)
	{
		return;
	}

	RefreshPerimeterIfNeeded(std::chrono::milliseconds(0));
	for (uint32_t i = static_cast<uint32_t>(entitiesById.size());
		 i < options.desiredCount; ++i)
	{
		const float halfExtent = std::max(options.halfExtent, 0.01F);
		const vec3 initialPosition = options.randomizeInitialPosition
										 ? RandomSpawnPosition(halfExtent)
										 : vec3(0.0F, 0.0F, 0.0F);

		AtlasEntity entity;
		entity.Entity_ID = MakeEntityId(i);
		entity.transform.world = 0;
		entity.transform.position = initialPosition;
		entity.transform.boundingBox.SetCenterExtents(
			initialPosition, vec3(halfExtent, halfExtent, halfExtent));
		entity.IsClient = false;
		entity.Client_ID = UUID();
		entity.Metadata.clear();

		LinearEntity linearEntity;
		linearEntity.entity = entity;
		linearEntity.halfExtent = halfExtent;
		linearEntity.velocity = options.randomizeInitialDirection
								 ? RandomVelocity(options.speedUnitsPerSec)
								 : vec3(options.speedUnitsPerSec, 0.0F, 0.0F);
		entitiesById[entity.Entity_ID] = std::move(linearEntity);
	}
}

void DebugEntityLinearBounceSimulator::AdoptSingleEntity(const AtlasEntity& entity)
{
	const auto existing = entitiesById.find(entity.Entity_ID);
	const vec3 extents = entity.transform.boundingBox.halfExtents();
	const float halfExtent = std::max(
		0.01F, extents.x > 0.0F ? extents.x : kDefaultHalfExtent);

	LinearEntity linearEntity;
	linearEntity.entity = entity;
	linearEntity.halfExtent = halfExtent;
	linearEntity.velocity = existing != entitiesById.end()
								? existing->second.velocity
								: RandomVelocity(kDefaultSpeedUnitsPerSec);

	entitiesById[entity.Entity_ID] = std::move(linearEntity);
}

void DebugEntityLinearBounceSimulator::RemoveEntity(const AtlasEntityID entityId)
{
	entitiesById.erase(entityId);
}

void DebugEntityLinearBounceSimulator::Tick(const TickOptions& options)
{
	const float deltaSeconds = std::clamp(options.deltaSeconds, 0.0F, 0.25F);
	RefreshPerimeterIfNeeded(options.perimeterRefreshInterval);

	for (auto& [entityId, linearEntity] : entitiesById)
	{
		(void)entityId;
		linearEntity.entity.transform.position += linearEntity.velocity * deltaSeconds;
		ReflectOnPerimeter(linearEntity);
		linearEntity.entity.transform.boundingBox.SetCenterExtents(
			linearEntity.entity.transform.position,
			vec3(linearEntity.halfExtent, linearEntity.halfExtent,
				 linearEntity.halfExtent));
	}
}

void DebugEntityLinearBounceSimulator::StoreStateSnapshots(
	std::string_view hashKey) const
{
	(void)hashKey;
}

std::vector<AtlasEntity> DebugEntityLinearBounceSimulator::GetEntitiesSnapshot() const
{
	std::vector<AtlasEntity> snapshots;
	snapshots.reserve(entitiesById.size());
	for (const auto& [_entityId, linearEntity] : entitiesById)
	{
		snapshots.push_back(linearEntity.entity);
	}
	return snapshots;
}

void DebugEntityLinearBounceSimulator::RefreshPerimeterIfNeeded(
	const std::chrono::milliseconds refreshInterval)
{
	const auto now = std::chrono::steady_clock::now();
	if (refreshInterval.count() > 0 &&
		lastPerimeterRefresh != std::chrono::steady_clock::time_point::min() &&
		now - lastPerimeterRefresh < refreshInterval)
	{
		return;
	}

	lastPerimeterRefresh = now;
	(void)RebuildPerimeterBounds();
}

bool DebugEntityLinearBounceSimulator::RebuildPerimeterBounds()
{
	AABB3f configuredWorldBounds;
	const bool haveConfiguredWorld =
		RebuildConfiguredWorldPerimeter(configuredWorldBounds);

	std::unordered_map<NetworkIdentity, GridShape> claimedBounds;
	HeuristicManifest::Get().GetAllClaimedBounds<GridShape>(
		claimedBounds);

	std::vector<GridShape> pendingBounds;
	HeuristicManifest::Get().GetAllPendingBounds<GridShape, std::string>(
		pendingBounds);

	bool haveAny = false;
	AABB3f combined;
	for (const auto& [_key, bound] : claimedBounds)
	{
		if (!haveAny)
		{
			combined.min = bound.aabb.min;
			combined.max = bound.aabb.max;
			haveAny = true;
			continue;
		}
		combined.expand(bound.aabb);
	}
	for (const auto& bound : pendingBounds)
	{
		if (!haveAny)
		{
			combined.min = bound.aabb.min;
			combined.max = bound.aabb.max;
			haveAny = true;
			continue;
		}
		combined.expand(bound.aabb);
	}
	if (haveConfiguredWorld)
	{
		if (!haveAny)
		{
			combined = configuredWorldBounds;
			haveAny = true;
		}
		else
		{
			combined.expand(configuredWorldBounds);
		}
	}

	if (!haveAny || !combined.valid())
	{
		worldPerimeterBounds = AABB3f::FromCenterExtents(
			vec3(0.0F, 0.0F, 0.0F), vec3(50.0F, 50.0F, 0.0F));
		worldPerimeterValid = true;
		if (logger)
		{
			logger->WarningFormatted(
				"[DebugLinearBounce] perimeter_fallback used=true "
				"claimed_count={} pending_count={} configured_world={} "
				"min={} max={}",
				claimedBounds.size(), pendingBounds.size(),
				haveConfiguredWorld ? "yes" : "no",
				glm::to_string(worldPerimeterBounds.min),
				glm::to_string(worldPerimeterBounds.max));
		}
		return false;
	}

	worldPerimeterBounds = combined;
	worldPerimeterValid = true;
	if (logger)
	{
		logger->WarningFormatted(
			"[DebugLinearBounce] perimeter_set claimed_count={} pending_count={} "
			"configured_world={} min={} max={}",
			claimedBounds.size(), pendingBounds.size(),
			haveConfiguredWorld ? "yes" : "no",
			glm::to_string(worldPerimeterBounds.min),
			glm::to_string(worldPerimeterBounds.max));
	}
	return true;
}

bool DebugEntityLinearBounceSimulator::RebuildConfiguredWorldPerimeter(
	AABB3f& outBounds) const
{
	if (HeuristicManifest::Get().GetActiveHeuristicType() !=
		IHeuristic::Type::eGridCell)
	{
		return false;
	}

	std::unordered_map<IBounds::BoundsID, ByteWriter> serializedBounds;
	GridHeuristic().SerializeBounds(serializedBounds);
	if (serializedBounds.empty())
	{
		return false;
	}

	bool haveAny = false;
	for (const auto& [_boundId, writer] : serializedBounds)
	{
		GridShape bound;
		const std::string payload(writer.as_string_view());
		ByteReader reader(payload);
		bound.Deserialize(reader);
		if (!haveAny)
		{
			outBounds.min = bound.aabb.min;
			outBounds.max = bound.aabb.max;
			haveAny = true;
			continue;
		}
		outBounds.expand(bound.aabb);
	}
	return haveAny && outBounds.valid();
}

vec3 DebugEntityLinearBounceSimulator::RandomVelocity(
	const float speedUnitsPerSec)
{
	const float speed = std::max(speedUnitsPerSec, 0.0F);
	std::uniform_real_distribution<float> directionDist(
		0.0F, static_cast<float>(2.0) * std::numbers::pi_v<float>);
	const float angle = directionDist(rng);
	return vec3(std::cos(angle) * speed, std::sin(angle) * speed, 0.0F);
}

vec3 DebugEntityLinearBounceSimulator::RandomSpawnPosition(const float halfExtent)
{
	if (!worldPerimeterValid)
	{
		return vec3(0.0F, 0.0F, 0.0F);
	}

	const float minX = worldPerimeterBounds.min.x + halfExtent;
	const float maxX = worldPerimeterBounds.max.x - halfExtent;
	const float minY = worldPerimeterBounds.min.y + halfExtent;
	const float maxY = worldPerimeterBounds.max.y - halfExtent;

	const float spawnX = minX <= maxX
							 ? std::uniform_real_distribution<float>(minX, maxX)(rng)
							 : worldPerimeterBounds.center().x;
	const float spawnY = minY <= maxY
							 ? std::uniform_real_distribution<float>(minY, maxY)(rng)
							 : worldPerimeterBounds.center().y;
	const float spawnZ = worldPerimeterBounds.center().z;
	return vec3(spawnX, spawnY, spawnZ);
}

void DebugEntityLinearBounceSimulator::ReflectOnPerimeter(
	LinearEntity& linearEntity) const
{
	if (!worldPerimeterValid)
	{
		return;
	}

	float& px = linearEntity.entity.transform.position.x;
	float& py = linearEntity.entity.transform.position.y;
	float& vx = linearEntity.velocity.x;
	float& vy = linearEntity.velocity.y;

	const float minX = worldPerimeterBounds.min.x + linearEntity.halfExtent;
	const float maxX = worldPerimeterBounds.max.x - linearEntity.halfExtent;
	const float minY = worldPerimeterBounds.min.y + linearEntity.halfExtent;
	const float maxY = worldPerimeterBounds.max.y - linearEntity.halfExtent;

	ReflectAxis(px, vx, minX, maxX);
	ReflectAxis(py, vy, minY, maxY);
}

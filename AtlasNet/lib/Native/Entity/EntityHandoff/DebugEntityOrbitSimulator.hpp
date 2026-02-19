#pragma once

// Debug-only simulator that spawns/adopts entities and moves them in circular
// motion to drive repeatable handoff trigger testing.

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Entity/Entity.hpp"
#include "Debug/Log.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntityOrbitSimulator
{
  public:
	static inline constexpr float kDefaultHalfExtent = 0.5F;
	static inline constexpr float kDefaultOrbitRadius = 12.0F;
	static inline constexpr float kDefaultOrbitAngularSpeedRadPerSec = 1.2F;

	struct SeedOptions
	{
		uint32_t desiredCount = 1;
		float halfExtent = kDefaultHalfExtent;
		float phaseStepRad = 0.0F;
	};

	struct OrbitOptions
	{
		float deltaSeconds = 0.0F;
		float radius = kDefaultOrbitRadius;
		float angularSpeedRadPerSec = kDefaultOrbitAngularSpeedRadPerSec;
	};

	DebugEntityOrbitSimulator(const NetworkIdentity& self,
							  std::shared_ptr<Log> inLogger);

	void Reset();
	void SeedEntities(const SeedOptions& options);
	void AdoptSingleEntity(const AtlasEntity& entity);
	void TickOrbit(const OrbitOptions& options);
	void StoreStateSnapshots(std::string_view hashKey) const;

	[[nodiscard]] std::vector<AtlasEntity> GetEntitiesSnapshot() const;
	[[nodiscard]] size_t Count() const { return entitiesById.size(); }

  private:
	struct OrbitEntity
	{
		AtlasEntity entity;
		float phaseOffsetRad = 0.0F;
	};

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	float orbitAngleRad = 0.0F;
	std::unordered_map<AtlasEntityID, OrbitEntity> entitiesById;
};

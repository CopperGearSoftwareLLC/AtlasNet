#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

#include "Entity/Entity.hpp"

// Common interface for debug entity simulators used by authority runtime.
class DebugEntitySimulator
{
  public:
	static inline constexpr float kDefaultHalfExtent = 0.5F;
	static inline constexpr float kDefaultOrbitRadius = 12.0F;
	static inline constexpr float kDefaultOrbitAngularSpeedRadPerSec = 1.2F;
	static inline constexpr float kDefaultSpeedUnitsPerSec = 18.0F;

	struct SeedOptions
	{
		uint32_t desiredCount = 1;
		float halfExtent = kDefaultHalfExtent;

		// Orbit-focused seed knobs.
		float phaseStepRad = 0.0F;
		float initialRadius = kDefaultOrbitRadius;
		bool randomizeInitialPhase = true;

		// Linear-focused seed knobs.
		float speedUnitsPerSec = kDefaultSpeedUnitsPerSec;
		bool randomizeInitialDirection = true;
		bool randomizeInitialPosition = true;
	};

	struct TickOptions
	{
		float deltaSeconds = 0.0F;

		// Orbit-focused tick knobs.
		float radius = kDefaultOrbitRadius;
		float angularSpeedRadPerSec = kDefaultOrbitAngularSpeedRadPerSec;

		// Linear-focused tick knobs.
		std::chrono::milliseconds perimeterRefreshInterval =
			std::chrono::milliseconds(1000);
	};

	virtual ~DebugEntitySimulator() = default;

	virtual void Reset() = 0;
	virtual void SeedEntities(const SeedOptions& options) = 0;
	virtual void AdoptSingleEntity(const AtlasEntity& entity) = 0;
	virtual void RemoveEntity(AtlasEntityID entityId) = 0;
	virtual void Tick(const TickOptions& options) = 0;
	virtual void StoreStateSnapshots(std::string_view hashKey) const = 0;

	[[nodiscard]] virtual std::vector<AtlasEntity> GetEntitiesSnapshot() const = 0;
	[[nodiscard]] virtual size_t Count() const = 0;
};


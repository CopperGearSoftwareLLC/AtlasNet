#pragma once

// Debug-only simulator that spawns/adopts entities, moves them linearly with
// velocity, and bounces velocity when world perimeter bounds are hit.

#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Debug/Log.hpp"
#include "Entity/EntityHandoff/DebugEntities/DebugEntitySimulator.hpp"
#include "Entity/Entity.hpp"
#include "Global/Types/AABB.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntityLinearBounceSimulator : public DebugEntitySimulator
{
  public:
	using SeedOptions = DebugEntitySimulator::SeedOptions;
	using TickOptions = DebugEntitySimulator::TickOptions;

	DebugEntityLinearBounceSimulator(const NetworkIdentity& self,
									 std::shared_ptr<Log> inLogger);

	void Reset() override;
	void SeedEntities(const SeedOptions& options) override;
	void AdoptSingleEntity(const AtlasEntity& entity) override;
	void RemoveEntity(AtlasEntityID entityId) override;
	void Tick(const TickOptions& options) override;
	void TickLinearBounce(const TickOptions& options) { Tick(options); }
	void StoreStateSnapshots(std::string_view hashKey) const override;

	[[nodiscard]] std::vector<AtlasEntity> GetEntitiesSnapshot() const override;
	[[nodiscard]] size_t Count() const override { return entitiesById.size(); }

  private:
	struct LinearEntity
	{
		AtlasEntity entity;
		vec3 velocity = vec3(0.0F, 0.0F, 0.0F);
		float halfExtent = kDefaultHalfExtent;
	};

	void RefreshPerimeterIfNeeded(std::chrono::milliseconds refreshInterval);
	[[nodiscard]] bool RebuildPerimeterBounds();
	[[nodiscard]] bool RebuildConfiguredWorldPerimeter(AABB3f& outBounds) const;
	[[nodiscard]] vec3 RandomVelocity(float speedUnitsPerSec);
	[[nodiscard]] vec3 RandomSpawnPosition(float halfExtent);
	void ReflectOnPerimeter(LinearEntity& linearEntity) const;

	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	std::unordered_map<AtlasEntityID, LinearEntity> entitiesById;
	std::mt19937_64 rng;
	AABB3f worldPerimeterBounds;
	bool worldPerimeterValid = false;
	std::chrono::steady_clock::time_point lastPerimeterRefresh =
		std::chrono::steady_clock::time_point::min();
};

#pragma once

// Debug-only simulator that spawns/adopts entities and moves them in circular
// motion to drive repeatable handoff trigger testing.

#include <cstdint>
#include <memory>
#include <random>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Entity/EntityHandoff/DebugEntities/DebugEntitySimulator.hpp"
#include "Entity/Entity.hpp"
#include "Debug/Log.hpp"
#include "Network/NetworkIdentity.hpp"

class DebugEntityOrbitSimulator : public DebugEntitySimulator
{
  public:
	using SeedOptions = DebugEntitySimulator::SeedOptions;
	using TickOptions = DebugEntitySimulator::TickOptions;
	using OrbitOptions = DebugEntitySimulator::TickOptions;

	DebugEntityOrbitSimulator(const NetworkIdentity& self,
							  std::shared_ptr<Log> inLogger);

	void Reset() override;
	void SeedEntities(const SeedOptions& options) override;
	void AdoptSingleEntity(const AtlasEntity& entity) override;
	void RemoveEntity(AtlasEntityID entityId) override;
	void Tick(const TickOptions& options) override;
	void TickOrbit(const TickOptions& options) { Tick(options); }
	void StoreStateSnapshots(std::string_view hashKey) const override;

	[[nodiscard]] std::vector<AtlasEntity> GetEntitiesSnapshot() const override;
	[[nodiscard]] size_t Count() const override { return entitiesById.size(); }

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
	std::mt19937_64 rng;
};

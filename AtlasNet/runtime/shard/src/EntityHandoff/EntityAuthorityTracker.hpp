#pragma once

// Stateful authority container for entities currently owned by this shard,
// including authoritative/passing state and current snapshot data.

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Entity.hpp"
#include "Log.hpp"
#include "Network/NetworkIdentity.hpp"

class EntityAuthorityTracker
{
  public:
	enum class AuthorityState : uint8_t
	{
		eAuthoritative = 0,
		ePassing = 1
	};

	struct AuthorityEntry
	{
		AtlasEntity entitySnapshot;
		AuthorityState authorityState = AuthorityState::eAuthoritative;
		std::optional<NetworkIdentity> passingTo;
	};

	struct AuthorityTelemetryRow
	{
		AtlasEntity::EntityID entityId = 0;
		NetworkIdentity owner;
		AtlasEntity entitySnapshot;
		uint16_t world = 0;
		vec3 position = vec3(0.0F, 0.0F, 0.0F);
		bool isClient = false;
		AtlasEntity::ClientID clientId = 0;
	};

	EntityAuthorityTracker(const NetworkIdentity& self,
						   std::shared_ptr<Log> inLogger);

	void Reset();
	void SetOwnedEntities(const std::vector<AtlasEntity>& ownedEntities);
	void CollectTelemetryRows(std::vector<AuthorityTelemetryRow>& outRows) const;
	void DebugLogTrackedEntities() const;
	[[nodiscard]] std::vector<AtlasEntity> GetOwnedEntitySnapshots() const;
	bool MarkPassing(AtlasEntity::EntityID entityId,
					 const NetworkIdentity& passingTarget);
	void MarkAuthoritative(AtlasEntity::EntityID entityId);

	void SetAuthorityState(AtlasEntity::EntityID entityId, AuthorityState state,
						   const std::optional<NetworkIdentity>& passingTo = std::nullopt);
	void RemoveEntity(AtlasEntity::EntityID entityId);

	[[nodiscard]] size_t Count() const { return authorityByEntityId.size(); }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	std::unordered_map<AtlasEntity::EntityID, AuthorityEntry> authorityByEntityId;
};

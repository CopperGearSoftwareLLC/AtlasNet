#pragma once

// Stateful authority container for entities currently owned by this shard,
// including authoritative/passing state and current snapshot data.

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "Client/Client.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/UUID.hpp"
#include "Debug/Log.hpp"
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
		AtlasEntityID entityId = 0;
		NetworkIdentity owner;
		AtlasEntity entitySnapshot;
		uint16_t world = 0;
		vec3 position = vec3(0.0F, 0.0F, 0.0F);
		bool isClient = false;
		ClientID clientId = UUID();
	};

	EntityAuthorityTracker(const NetworkIdentity& self,
						   std::shared_ptr<Log> inLogger);

	void Reset();
	void SetOwnedEntities(const std::vector<AtlasEntity>& ownedEntities);
	void CollectTelemetryRows(std::vector<AuthorityTelemetryRow>& outRows) const;
	void DebugLogTrackedEntities() const;
	[[nodiscard]] std::vector<AtlasEntity> GetOwnedEntitySnapshots() const;
	bool MarkPassing(AtlasEntityID entityId,
					 const NetworkIdentity& passingTarget);
	void MarkAuthoritative(AtlasEntityID entityId);

	void SetAuthorityState(AtlasEntityID entityId, AuthorityState state,
						   const std::optional<NetworkIdentity>& passingTo = std::nullopt);
	void RemoveEntity(AtlasEntityID entityId);

	[[nodiscard]] size_t Count() const { return authorityByEntityId.size(); }

  private:
	NetworkIdentity selfIdentity;
	std::shared_ptr<Log> logger;
	std::unordered_map<AtlasEntityID, AuthorityEntry> authorityByEntityId;
};

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
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

	EntityAuthorityTracker(const NetworkIdentity& self,
						   std::shared_ptr<Log> inLogger);

	void Reset();
	void SetOwnedEntities(const std::vector<AtlasEntity>& ownedEntities);
	void StoreAuthorityStateSnapshots(std::string_view hashKey);
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
	std::unordered_set<std::string> lastPublishedFields;
};

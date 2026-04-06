#pragma once

#include "atlasnet/core/entity/Entity.hpp"
#include "entt/entity/fwd.hpp"
#include <boost/bimap.hpp>
#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
namespace AtlasNet
{
namespace Entity
{

class EntityLedger
{
  using EnTTEntityID = EntityTable::entity_type;

public:
  enum ActorTransferMode
  {
    eRPC,
    eMessage
  };
  struct Config
  {
    ActorTransferMode transferMode = ActorTransferMode::eRPC;
  };
  EntityLedger(const Config& config) : _config(config) {}

  class ReadAccess
  {
  public:
    explicit ReadAccess(const EntityLedger& ledger)
        : _ledger(ledger), _lock(ledger._mutex)
    {
    }

    bool EntityExists(const EntityID& id) const
    {
      return _ledger._entityExists(id);
    }

    bool IsClient(const EntityID& id) const
    {
      return _ledger._isClient(id);
    }
    bool IsActor(const EntityID& id) const
    {
      return _ledger._isActor(id);
    }
    const Entity::Components::EntityInfo&
    GetEntityInfo(const EntityID& id) const
    {
      return _ledger._GetEntityInfo(id);
    }

  private:
    const EntityLedger& _ledger;
    std::shared_lock<std::shared_mutex> _lock;
  };
  class WriteAccess
  {
  public:
    explicit WriteAccess(EntityLedger& ledger)
        : _ledger(ledger), _lock(ledger._mutex)
    {
    }

    bool EntityExists(const EntityID& id) const
    {
      return _ledger._entityExists(id);
    }

    bool IsClient(const EntityID& id) const
    {
      return _ledger._isClient(id);
    }
    bool IsActor(const EntityID& id) const
    {
      return _ledger._isActor(id);
    }

    EntityID CreateEntity(const Components::EntityInfo& info)
    {
      EntityID id = EntityID(UUID::Generate());
      _ledger._createEntity(id, info);
      return id;
    }
    void RemoveEntity(const EntityID& id)
    {
      _ledger._removeEntity(id);
    }
    Entity::Components::EntityInfo GetEntityInfo(const EntityID& id)
    {
      return _ledger._GetEntityInfo(id);
    }

  private:
    EntityLedger& _ledger;
    std::unique_lock<std::shared_mutex> _lock;
  };
  WriteAccess GetWriteAccess()
  {
    return WriteAccess(*this);
  }
  ReadAccess GetReadAccess() const
  {
    return ReadAccess(*this);
  }

protected:
  EnTTEntityID _createEntity(const EntityID& id,
                             const Entity::Components::EntityInfo& info)
  {
    EnTTEntityID enttId = entityTable.create();
    IDMapping.insert({id, enttId});
    entityTable.emplace<Entity::Components::EntityInfo>(enttId, info);
    return enttId;
  }
  bool _entityExists(const EntityID& id) const
  {
    return IDMapping.left.find(id) != IDMapping.left.end();
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  bool _EntityHasComponent(const EntityID& id) const
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    return entityTable.all_of<ComponentType>(enttId);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  void _EntityAddComponent(const EntityID& id, const ComponentType& component)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity already has component");
    }
    entityTable.emplace<ComponentType>(enttId, component);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  void _EntityRemoveComponent(const EntityID& id)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (!entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity does not have component");
    }
    entityTable.remove<ComponentType>(enttId);
  }
  template <typename ComponentType>
    requires std::derived_from<ComponentType,
                               Entity::Components::EntityComponent>
  ComponentType _EntityGetComponent(const EntityID& id)
  {
    EnTTEntityID enttId = GetEnTTEntityID(id);
    if (!entityTable.all_of<ComponentType>(enttId))
    {
      throw std::runtime_error("Entity does not have component");
    }
    return entityTable.get<ComponentType>(enttId);
  }
  bool _isClient(const EntityID& id) const
  {
    return _EntityHasComponent<Entity::Components::ClientInfo>(id);
  }
  bool _isActor(const EntityID& id) const
  {
    return _EntityHasComponent<Entity::Components::ActorInfo>(id);
  }
  void _setEntityActor(const EntityID& id,
                       const Entity::Components::ActorInfo& actorInfo)
  {
    _EntityAddComponent(id, actorInfo);
  }
  void _setEntityClient(const EntityID& id,
                       const Entity::Components::ClientInfo& clientInfo)
  {
    _EntityAddComponent(id, clientInfo);
  }
  void _removeEntity(const EntityID& id)
  {
    entt::entity enttId = GetEnTTEntityID(id);
    entityTable.destroy(enttId);
    IDMapping.left.erase(id);
  }

  Entity::Components::EntityInfo _GetEntityInfo(const EntityID& id) const
  {
    EnTTEntityID _id = GetEnTTEntityID(id);
    auto& info = entityTable.get<Entity::Components::EntityInfo>(_id);
    // Use info as needed
    return info;
  }
 

private:
  [[nodiscard]] EnTTEntityID GetEnTTEntityID(const EntityID& id) const
  {
    auto it = IDMapping.left.find(id);
    if (it != IDMapping.left.end())
    {
      return it->second;
    }
    throw std::runtime_error("EntityID not found in mapping");
  }
  const Config _config;
  boost::bimap<EntityID, EnTTEntityID> IDMapping;
  // std::unordered_map<EntityID, typename Tp>
  EntityTable entityTable;
  mutable std::shared_mutex _mutex;
};
} // namespace Entity
} // namespace AtlasNet
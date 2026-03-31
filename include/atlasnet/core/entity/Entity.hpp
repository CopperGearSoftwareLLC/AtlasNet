#pragma once
#include "atlasnet/core/UUID.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
#include "atlasnet/core/pch.hpp"
#include <variant>
namespace AtlasNet {

struct EntityIDTag {};
struct ClientIDTag {};

using EntityID = StrongUUID<EntityIDTag>;
using ClientID = StrongUUID<ClientIDTag>;
using WorldID = uint32_t;

struct Entity {
  EntityID id;
  WorldID worldId;
  vec3 position;
  std::variant<std::monostate, BoxCollider, SphereCollider> collider;

  explicit Entity(EntityID id) : id(std::move(id)) {}
};

struct Client : Entity {
  ClientID clientId;
  explicit Client(EntityID id, ClientID clientId)
      : Entity(std::move(id)), clientId(std::move(clientId)) {}
};
} // namespace AtlasNet
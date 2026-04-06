#pragma once
#include "atlasnet/core/UUID.hpp"
#include "atlasnet/core/entity/collider/Collider.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "boost/container/small_vector.hpp"
#include "entt/entity/fwd.hpp"
#include <entt/entt.hpp>
#include <variant>
namespace AtlasNet
{
namespace Entity
{

struct EntityIDTag
{
};
struct ClientIDTag
{
};
using EntityID = StrongUUID<EntityIDTag>;

#define ENTT_STANDARD_CPP
using ClientID = StrongUUID<ClientIDTag>;
using WorldID = uint32_t;
struct Transform
{
  vec3 position;
};
struct Location
{
  WorldID worldId;
  Transform transform;
};
namespace Components
{
  struct EntityComponent {};
struct EntityInfo : public EntityComponent
{
  EntityID id;
  Location location;
};
struct ActorInfo : public EntityComponent
{
  int pad;
  // Data is not required because we dont store, we serialize on transform from
  // IAtlasNetShard boost::container::small_vector<uint8_t, 256> payload;
};
struct ClientInfo : public EntityComponent
{
  ClientID id;
};
struct Collider : public EntityComponent
{
  std::variant<BoxCollider, SphereCollider> collider;
};

} // namespace Components

// using EntityTabl2 = entt::basic_registry<Entitye>;
using EntityTable = entt::registry;
} // namespace Entity
} // namespace AtlasNet
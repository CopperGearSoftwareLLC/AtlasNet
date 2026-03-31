#pragma once
#include "atlasnet/core/Singleton.hpp"
#include "atlasnet/core/UUID.hpp"
namespace AtlasNet {

struct ContainerIDTag {};
using ContainerID = StrongUUID<ContainerIDTag>;

class ContainerInfo : public Singleton<ContainerInfo> {

  ContainerID id;

public:
  ContainerInfo() : id(ContainerID::Generate()) {}

    const ContainerID &GetID() const { return id; }
};
} // namespace AtlasNet
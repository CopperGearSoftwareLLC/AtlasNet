#pragma once

#include "atlasnet/core/geometry/AABB.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
class ICollider {
public:
  virtual ~ICollider() = default;
};

class NoCollider : public ICollider {};
class BoxCollider : public ICollider {
  AtlasNet::AABB3f bounds;

public:
  explicit BoxCollider(vec3 size) : bounds(-size * 0.5f, size * 0.5f) {}
  explicit BoxCollider(float width, float height, float depth)
      : bounds(vec3(-width, -height, -depth) * 0.5f,
               vec3(width, height, depth) * 0.5f) {}
};


class SphereCollider : public ICollider {
  float radius; 
public:
    explicit SphereCollider(float radius) : radius(radius) {}
};

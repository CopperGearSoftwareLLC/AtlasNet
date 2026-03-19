#pragma once

#include "Global/Types/AABB.hpp"
struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir;
};

Ray BuildRayFromNDC(const glm::vec2& ndc, const glm::mat4& viewProj)
{
    glm::mat4 invVP = glm::inverse(viewProj);

    // NDC space (-1 to 1)
    glm::vec4 nearPoint = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    glm::vec4 farPoint  = invVP * glm::vec4(ndc.x, ndc.y,  1.0f, 1.0f);

    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    Ray ray;
    ray.origin = glm::vec3(nearPoint);
    ray.dir    = glm::normalize(glm::vec3(farPoint - nearPoint));
    return ray;
}
bool RayAABB(const Ray& ray, const AABB3f& aabb, float& t)
{
    float tmin = (aabb.min.x - ray.origin.x) / ray.dir.x;
    float tmax = (aabb.max.x - ray.origin.x) / ray.dir.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabb.min.y - ray.origin.y) / ray.dir.y;
    float tymax = (aabb.max.y - ray.origin.y) / ray.dir.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    tmin = std::max(tmin, tymin);
    tmax = std::min(tmax, tymax);

    float tzmin = (aabb.min.z - ray.origin.z) / ray.dir.z;
    float tzmax = (aabb.max.z - ray.origin.z) / ray.dir.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    tmin = std::max(tmin, tzmin);
    tmax = std::min(tmax, tzmax);

    t = tmin;
    return t >= 0.0f;
}
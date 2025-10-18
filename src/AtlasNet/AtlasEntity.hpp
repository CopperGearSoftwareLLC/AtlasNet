#pragma once

using AtlasEntityID = uint32;
struct AtlasEntity
{
    AtlasEntityID ID;
    //uint32 a;
    vec3 Position;
    vec3 Rotation;
    vec3 Scale;
    bool IsSpawned;
};
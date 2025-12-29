#pragma once
#include <pch.hpp>
inline Json to_json( const vec3& v)
{
    Json j = Json{
        {"x", v.x},
        {"y", v.y},
        {"z", v.z}
    };
    return j;
}

inline vec3 from_json(const Json& j)
{
    vec3 v;
    v.x = j.value("x", 0.0f);
    v.y = j.value("y", 0.0f);
    v.z = j.value("z", 0.0f);
    return v;
}

inline void to_json(Json& j, const vec3& v)
{
    j = Json{
        {"x", v.x},
        {"y", v.y},
        {"z", v.z}
    };
}

inline void from_json(const Json& j, vec3& v)
{
    v.x = j.value("x", 0.0f);
    v.y = j.value("y", 0.0f);
    v.z = j.value("z", 0.0f);
}

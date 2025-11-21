#include "AtlasEntity.hpp"
#include "Utils/JsonUtils.hpp"
Json AtlasEntity::ToJson() const
{

    Json j = Json{
        {"ID", ID},
        {"Position", to_json(Position)},
        {"Rotation", to_json(Rotation)},
        {"Scale", to_json(Scale)},
        {"IsSpawned", IsSpawned},
        {"IsPlayer", IsPlayer}
    };
    return j;
}

AtlasEntity AtlasEntity::FromJson(const Json &j)
{
    AtlasEntity e;
        e.ID = j.value("ID", 0u);
    e.Position = from_json(j["Position"]);
    e.Rotation = from_json(j["Rotation"]);
    e.Scale = from_json(j["Scale"]);
    e.IsSpawned = j.value("IsSpawned", false);
    e.IsPlayer  = j.value("IsPlayer", true); // default in struct
    return e;
}

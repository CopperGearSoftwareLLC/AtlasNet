#ifndef ATLAS_ENTITY_HPP
#define ATLAS_ENTITY_HPP

#pragma once
#include "pch.hpp"
using AtlasEntityID = uint32;
struct AtlasEntity
{
    AtlasEntityID ID;
    vec3 Position;
    vec3 Rotation;
    vec3 Scale;
    bool IsSpawned;
    bool IsPlayer = true;

    Json ToJson() const;
    static AtlasEntity FromJson(const Json& json);
    //add a to string and from string 
    std::string ToString() const
    {
        return std::to_string(ID) + ":" + 
               std::to_string(Position.x) + "," + 
               std::to_string(Position.y) + "," + 
               std::to_string(Position.z);
    }
    static AtlasEntity FromString(const std::string& str)
    {
        AtlasEntity entity;
        size_t colonPos = str.find(':');
        if (colonPos == std::string::npos) throw std::invalid_argument("Invalid AtlasEntity string format");

        entity.ID = static_cast<AtlasEntityID>(std::stoul(str.substr(0, colonPos)));
        
        // Parse position: "x,y,z"
        std::string posStr = str.substr(colonPos + 1);
        size_t comma1 = posStr.find(',');
        size_t comma2 = posStr.find(',', comma1 + 1);
        
        if (comma1 == std::string::npos || comma2 == std::string::npos) {
            throw std::invalid_argument("Invalid position format in AtlasEntity string");
        }
        
        entity.Position.x = std::stof(posStr.substr(0, comma1));
        entity.Position.y = std::stof(posStr.substr(comma1 + 1, comma2 - (comma1 + 1)));
        entity.Position.z = std::stof(posStr.substr(comma2 + 1));
        
        return entity;
    }
};

#endif // ATLAS_ENTITY_HPP
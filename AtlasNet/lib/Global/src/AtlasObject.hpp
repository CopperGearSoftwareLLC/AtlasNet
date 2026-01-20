#pragma once

#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
class AtlasObject
{
    public:
    virtual void Serialize(ByteWriter& bw) const = 0;
    virtual void Deserialize(ByteReader& br) =0;
};
#pragma once

#include <steam/steamtypes.h>

#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Network/NetworkIdentity.hpp"

using EventTypeID = uint64;
class IEvent
{
	NetworkIdentity dispatcher;
	virtual void Serialize(ByteWriter& bw) = 0;
	virtual void Deserialize(ByteReader& br) = 0;
};

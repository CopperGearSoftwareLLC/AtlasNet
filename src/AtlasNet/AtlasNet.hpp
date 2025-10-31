#pragma once
//#include "Server/AtlasNetServer.hpp"
//#include "Client/AtlasNetClient.hpp"

enum class AtlasNetMessageHeader : uint8_t
{
    EntityUpdate = 0,
    EntityIncoming = 1,
    EntityOutgoing = 2,
};
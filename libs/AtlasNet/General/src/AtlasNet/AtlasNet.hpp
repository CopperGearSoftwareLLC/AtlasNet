#pragma once
//#include "Server/AtlasNetServer.hpp"
//#include "Client/AtlasNetClient.hpp"
#include "misc/Singleton.hpp"
#include "pch.hpp"
#include "Debug/Log.hpp"
enum class AtlasNetMessageHeader : uint8_t
{   
    Null = 255,
    EntityUpdate = 0,
    EntityIncoming = 1,
    EntityOutgoing = 2,
    FetchGridShape = 3
};
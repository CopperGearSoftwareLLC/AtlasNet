#include "pch.hpp"
#include "AtlasNet/AtlasNetInterface.hpp"
#include "Singleton.hpp"
class AtlasNetClient: public AtlasNetInterface, public Singleton<AtlasNetClient>
{

};
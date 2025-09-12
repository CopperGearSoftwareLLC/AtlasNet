#include "Interlink.hpp"

void Interlink::Initialize(const InterlinkProperties& Properties)
{
    ThisType = Properties.Type;
    ASSERT(ThisType!= InterlinkType::eInvalid, "Invalid Interlink Type");
    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
    {
        std::cerr << "GameNetworkingSockets_Init failed: " << errMsg << std::endl;
        return;
    }

    networkInterface = SteamNetworkingSockets();
    SteamNetworkingIPAddr serverLocalAddr;
		serverLocalAddr.Clear();
}
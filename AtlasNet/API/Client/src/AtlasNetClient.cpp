#include "AtlasNetClient.hpp"

#include "Client/ClientLink.hpp"
#include "Docker/DockerIO.hpp"
#include "Interlink/Interlink.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
void AtlasNetClient::Initialize(AtlasNetClient::InitializeProperties &props)
{
	logger->Debug("[AtlasNetClient] Initialize");
	NetworkIdentity baseIdentity{NetworkIdentityType::eGameClient};
	ClientLink::Get().Init();

	logger->DebugFormatted("[AtlasNetClient] establishing connection to {}",
						   props.AtlasNetProxyIP.ToString());
	ClientLink::Get().ConnectToAtlasNet(props.AtlasNetProxyIP);
}

void AtlasNetClient::Tick()
{
}

void AtlasNetClient::Shutdown()
{
	// Interlink::Get().Shutdown();
	// logger.reset();
}

#pragma once

#include "Client.hpp"
#include "Misc/Singleton.hpp"
class ClientManifest : public Singleton<ClientManifest>
{
	const std::string ClientID_2_IP_Hashtable = "ClientID to IP";
	const std::string ClientID_2_ProxyID_Hashtable = "ClientID to ProxyID";
	const std::string ProxyID_2_ClientIDs_Set = "Proxy_{}_Clients";

	void RegisterClient(const Client& client);
};
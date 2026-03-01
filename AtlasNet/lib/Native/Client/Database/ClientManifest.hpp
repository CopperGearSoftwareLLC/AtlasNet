#pragma once

#include "Client/Client.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkIdentity.hpp"
class ClientManifest : public Singleton<ClientManifest>
{
	const std::string ClientID_2_IP_Hashtable = "Client::ClientID2IP";
	const std::string ClientID_2_EntityID_Hashtable = "Client::ClientID2EntityID";
	const std::string ClientID_2_ProxyID_Hashtable = "ClientID2ProxyID";
	const std::string ProxyID_2_ClientIDs_Set = "Proxy_{}_Clients";

   private:
	void __ClientSetIP(const ClientID& c, const IPAddress& address);
	void __ClientSetProxy(const ClientID& c, const NetworkIdentity& idProxy);
	std::optional<NetworkIdentity> __ClientGetProxy(const ClientID& client_ID);
	void __ProxyRemoveClient(const NetworkIdentity& idProxy, const ClientID& ID);
	void __ProxyAddClient(const NetworkIdentity& idProxy, const ClientID& ID);
	void __ProxyRemoveClients(const NetworkIdentity& idProxy, const std::span<ClientID>& ID);

   public:
	void RegisterClient(const Client& client);
	void GetProxyClients(const NetworkIdentity& id, std::vector<ClientID>& clients);
	std::optional<NetworkIdentity> GetClientProxy(const ClientID& client_ID) {}
	void AssignProxyClient(const ClientID& cid, const NetworkIdentity& ID);
	
};
#pragma once

#include <optional>

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
class ClientManifest : public Singleton<ClientManifest>
{
	Log logger = Log("ClientManifest");
	const std::string ClientID_2_IP_Hashtable = "Client::ClientID -> IP";
	const std::string ClientID_2_EntityID_Hashtable = "Client::ClientID -> EntityID";
	const std::string ClientID_2_Proxy_Hashtable = "Client::Routing::ClientID -> Proxy";
	const std::string ClientID_2_Shard_Hashtable = "Client::Routing::ClientID -> Shard";
	constexpr static const char* Shard_ClientIDs = "Client::Routing::Shard::{}_Clients";
	constexpr static const char* Proxy_ClientIDs = "Client::Routing::Proxy::{}_Clients";
	std::string GetProxyClientListKey(const NetworkIdentity& ID) const
	{
		ASSERT(ID.Type == NetworkIdentityType::eProxy, "Invalid ID");
		return std::format(Proxy_ClientIDs, UUIDGen::ToString(ID.ID));
	}
	std::string GetShardClientListKey(const NetworkIdentity& ID) const
	{
		ASSERT(ID.Type == NetworkIdentityType::eShard, "Invalid ID");

		return std::format(Shard_ClientIDs, UUIDGen::ToString(ID.ID));
	}

   private:
	void __ClientSetIP(const ClientID& c, const IPAddress& address);

   public:
	std::optional<AtlasEntityID> GetClientEntityID(const ClientID& clientid);
	void InsertClient(const Client& client);
	void AssignClientEntity(const ClientID& clientid, const AtlasEntityID& entityid);
	void RemoveClient(const ClientID& clientID);
	void GetProxyClients(const NetworkIdentity& id, std::vector<ClientID>& clients);
	std::optional<NetworkIdentity> GetClientProxy(const ClientID& client_ID);
	void AssignProxyClient(const ClientID& cid, const NetworkIdentity& ID);

	std::optional<NetworkIdentity> GetClientShard(const ClientID& client_ID);
	void AssignShardClient(const ClientID& cid, const NetworkIdentity& ID);
};
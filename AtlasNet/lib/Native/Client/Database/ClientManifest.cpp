#include "ClientManifest.hpp"

#include <boost/container/small_vector.hpp>
#include <optional>

#include "Client/Client.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"

void ClientManifest::__ClientSetIP(const ClientID& c, const IPAddress& address)
{
	ByteWriter IDWrite;
	ByteWriter IPWrite;
	IDWrite.uuid(c);
	address.Serialize(IPWrite);
	const auto result = InternalDB::Get()->HSet(ClientID_2_IP_Hashtable, IDWrite.as_string_view(),
												IPWrite.as_string_view());
}

void ClientManifest::InsertClient(const Client& client)
{
	__ClientSetIP(client.ID, client.ip);
}
void ClientManifest::RemoveClient(const ClientID& clientID)
{
	ByteWriter bw;
	bw.uuid(clientID);
	const auto result1 = InternalDB::Get()->HDel(ClientID_2_IP_Hashtable, {bw.as_string_view()});
	const auto result2 =
		InternalDB::Get()->HDel(ClientID_2_EntityID_Hashtable, {bw.as_string_view()});
}
void ClientManifest::AssignClientEntity(const ClientID& clientid, const AtlasEntityID& entityid)
{
	ByteWriter keyWrite;
	ByteWriter valueWrite;
	keyWrite.uuid(clientid);
	valueWrite.uuid(entityid);
	const auto result = InternalDB::Get()->HSet(
		ClientID_2_EntityID_Hashtable, keyWrite.as_string_view(), valueWrite.as_string_view());
}
std::optional<AtlasEntityID> ClientManifest::GetClientEntityID(const ClientID& clientid)
{
	ByteWriter keyWrite;
	keyWrite.uuid(clientid);
	const std::optional<std::string> value =
		InternalDB::Get()->HGet(ClientID_2_EntityID_Hashtable, keyWrite.as_string_view());
	if (!value.has_value())
		return std::nullopt;

	ByteReader br(*value);
	return br.uuid();
}

void ClientManifest::GetProxyClients(const NetworkIdentity& id, std::vector<ClientID>& clients)
{
	const std::string Key = GetProxyClientListKey(id);
	const std::vector<std::string> members = InternalDB::Get()->SMembers(Key);
	clients.clear();
	for (const auto& m : members)
	{
		ByteReader br(m);
		clients.emplace_back(br.uuid());
	}
}
std::optional<NetworkIdentity> ClientManifest::GetClientProxy(const ClientID& client_ID)
{
	ByteWriter keyWrite;
	keyWrite.uuid(client_ID);

	const std::optional<std::string> ret =
		InternalDB::Get()->HGet(ClientID_2_Proxy_Hashtable, keyWrite.as_string_view());
	/* logger.DebugFormatted("GetClientProxy responded for client {}: \n{}",
						  UUIDGen::ToString(client_ID), ret.value_or("NO RESPONSE")); */
	if (!ret.has_value())
		return std::nullopt;

	ByteReader br(ret.value());

	return NetworkIdentity::MakeIDProxy(br.uuid());
}
void ClientManifest::AssignProxyClient(const ClientID& cid, const NetworkIdentity& ID)
{
	{
		const std::string ClientListKey = GetProxyClientListKey(ID);
		ByteWriter valueWrite;
		valueWrite.uuid(cid);
		InternalDB::Get()->SAdd(ClientListKey, {valueWrite.as_string_view()});
	}
	{
		ByteWriter keywrite, valuewrite;
		keywrite.uuid(cid);
		valuewrite.uuid(ID.ID);
		const auto ret = InternalDB::Get()->HSet(
			ClientID_2_Proxy_Hashtable, keywrite.as_string_view(), valuewrite.as_string_view());
	}
}
void ClientManifest::AssignShardClient(const ClientID& cid, const NetworkIdentity& ID)
{
	{
		const std::string ClientListKey = GetShardClientListKey(ID);
		ByteWriter valueWrite;
		valueWrite.uuid(cid);
		InternalDB::Get()->SAdd(ClientListKey, {valueWrite.as_string_view()});
	}
	{
		ByteWriter keywrite, valuewrite;
		keywrite.uuid(cid);
		valuewrite.uuid(ID.ID);
		const auto ret = InternalDB::Get()->HSet(
			ClientID_2_Shard_Hashtable, keywrite.as_string_view(), valuewrite.as_string_view());
	}
}
std::optional<NetworkIdentity> ClientManifest::GetClientShard(const ClientID& client_ID)
{
	ByteWriter keyWrite;
	keyWrite.uuid(client_ID);

	const std::optional<std::string> ret =
		InternalDB::Get()->HGet(ClientID_2_Shard_Hashtable, keyWrite.as_string_view());
	if (!ret.has_value())
		return std::nullopt;

	ByteReader br(ret.value());

	return NetworkIdentity::MakeIDShard(br.uuid());
}

#include "ClientManifest.hpp"

#include <boost/container/small_vector.hpp>
#include <optional>

#include "Client.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
void ClientManifest::RegisterClient(const Client& client)
{
	__ClientSetIP(client.ID, client.ip);
}
void ClientManifest::__ClientSetIP(const ClientID& c, const IPAddress& address)
{
	ByteWriter IDWrite;
	ByteWriter IPWrite;
	IDWrite.uuid(c);
	address.Serialize(IPWrite);
	const auto result = InternalDB::Get()->HSet(ClientID_2_IP_Hashtable,
												IDWrite.as_string_view(),
												IPWrite.as_string_view());
}
void ClientManifest::__ClientSetProxy(const ClientID& c,
									  const NetworkIdentity& idProxy)
{
	ByteWriter IDWrite;
	ByteWriter proxywrite;

	IDWrite.uuid(c);
	idProxy.Serialize(proxywrite);
	const auto result = InternalDB::Get()->HSet(ClientID_2_ProxyID_Hashtable,
												IDWrite.as_string_view(),
												proxywrite.as_string_view());
}
void ClientManifest::AssignProxyClient(const ClientID& cid,
									   const NetworkIdentity& ID)
{
	ASSERT(ID.Type == NetworkIdentityType::eProxy, "Proxy must be proxy type");
	const auto OriginalProxy = __ClientGetProxy(cid);
	if (OriginalProxy.has_value())
	{
		__ProxyRemoveClient(OriginalProxy.value(), cid);
	}
	__ClientSetProxy(cid, ID);
	__ProxyAddClient(ID, cid);
}

std::optional<NetworkIdentity> ClientManifest::__ClientGetProxy(
	const ClientID& client_ID)
{
	ByteWriter bw;
	bw.uuid(client_ID);
	const auto result = InternalDB::Get()->HGet(ClientID_2_ProxyID_Hashtable,
												bw.as_string_view());

	if (result.has_value())
	{
		ByteReader br(result.value());
		NetworkIdentity id;
		id.Deserialize(br);
		return id;
	}
	return std::nullopt;
}
void ClientManifest::__ProxyRemoveClient(const NetworkIdentity& idProxy,
										 const ClientID& ID)
{
	ByteWriter bw;
	bw.uuid(ID);
	InternalDB::Get()->SRem(ProxyID_2_ClientIDs_Set, {bw.as_string_view()});
}
void ClientManifest::__ProxyRemoveClients(const NetworkIdentity& idProxy,
										  const std::span<ClientID>& IDs)
{
	boost::container::small_vector<ByteWriter, 15> byteWriters;
	byteWriters.resize(IDs.size());
	for (int i = 0; i < IDs.size(); i++)
	{
		byteWriters[i].uuid(IDs[i]);
	}

	std::vector<std::string_view> values;
	values.resize(IDs.size());
	for (int i = 0; i < values.size(); i++)
	{
		values[i] = byteWriters[i].as_string_view();
	}

	InternalDB::Get()->SRem(ProxyID_2_ClientIDs_Set, values);
}
void ClientManifest::__ProxyAddClient(const NetworkIdentity& idProxy,
									  const ClientID& ID)
{
	ByteWriter bw;
	bw.uuid(ID);
	InternalDB::Get()->SAdd(ProxyID_2_ClientIDs_Set, {bw.as_string_view()});
}

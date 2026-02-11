#include "ServerRegistry.hpp"

#include "Network/NetworkIdentity.hpp"
#include "InternalDB.hpp"
#include "Misc/String_utils.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

static const std::string kPublicSuffix = "_public";

void ServerRegistry::RegisterSelf(const NetworkIdentity &ID, IPAddress address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP, GetKeyOfIdentifier(ID),
							NukeString(address.ToString()));
}

void ServerRegistry::RegisterPublicAddress(const NetworkIdentity &ID, const IPAddress &address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP + kPublicSuffix, GetKeyOfIdentifier(ID),
							NukeString(address.ToString()));
}

void ServerRegistry::DeRegisterSelf(const NetworkIdentity &ID)
{
	const std::string key = GetKeyOfIdentifier(ID);
	InternalDB::Get()->HDel(HashTableNameID_IP, {key});
	InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix, {key});
}

const decltype(ServerRegistry::servers) &ServerRegistry::GetServers()
{
	servers.clear();
	const auto entries = InternalDB::Get()->HGetAll(HashTableNameID_IP);
	for (const auto &rawEntry : entries)
	{
		ServerRegistryEntry newEntry;
		NetworkIdentity id;
		ByteReader br(rawEntry.first);
		id.Deserialize(br);
		
		newEntry.identifier = id;
		newEntry.address.Parse(rawEntry.second);

		ASSERT(newEntry.identifier.Type != NetworkIdentityType::eInvalid,
			   "Invalid entry in server ServerRegistry");
		ASSERT(!servers.contains(newEntry.identifier), "Duplicate entry in server ServerRegistry?");
		servers.insert(std::make_pair(newEntry.identifier, newEntry));
	}
	return servers;
}

std::optional<IPAddress> ServerRegistry::GetIPOfID(const NetworkIdentity &ID)
{
	std::string ret =
		InternalDB::Get()->HGet(HashTableNameID_IP, GetKeyOfIdentifier(ID)).value();
	if (ret.empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(ret);
	return ip;
}

std::optional<IPAddress> ServerRegistry::GetPublicAddress(const NetworkIdentity &ID)
{
	std::string ret = InternalDB::Get()
						  ->HGet(HashTableNameID_IP + kPublicSuffix,GetKeyOfIdentifier(ID))
						  .value();
	if (ret.empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(ret);
	return ip;
}

bool ServerRegistry::ExistsInRegistry(const NetworkIdentity &ID) const
{
	ByteWriter bw;
	ID.Serialize(bw);
	return InternalDB::Get()->HExists(HashTableNameID_IP, GetKeyOfIdentifier(ID));
}

void ServerRegistry::ClearAll()
{
	InternalDB::Get()->DelKey(HashTableNameID_IP);
	InternalDB::Get()->DelKey(HashTableNameID_IP + kPublicSuffix);
}

ServerRegistry::ServerRegistry() {}
const std::string ServerRegistry::GetKeyOfIdentifier(const NetworkIdentity &ID) {
		ByteWriter bw;
	ID.Serialize(bw);
	std::string s(bw.as_string_view());
	return s;
}


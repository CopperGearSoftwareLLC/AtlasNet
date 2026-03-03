#include "ServerRegistry.hpp"

#include "Network/NetworkIdentity.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Global/Misc/String_utils.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Debug/Log.hpp"

static const std::string kPublicSuffix = "_public";

void ServerRegistry::RegisterSelf(const NetworkIdentity &ID, IPAddress address)
{
	Log logger("ServerRegistry");
	logger.DebugFormatted("RegisterSelf: {} at {}", ID.ToString(), address.ToString());
	InternalDB::Get()->HSet(HashTableNameID_IP, GetKeyOfIdentifier(ID),
							NukeString(address.ToString()));
}

void ServerRegistry::RegisterPublicAddress(const NetworkIdentity &ID, const IPAddress &address)
{
	Log logger("ServerRegistry");
	logger.DebugFormatted("RegisterPublicAddress: {} public {}", ID.ToString(),
						  address.ToString());
	InternalDB::Get()->HSet(HashTableNameID_IP + kPublicSuffix, GetKeyOfIdentifier(ID),
							NukeString(address.ToString()));
}

void ServerRegistry::DeRegisterSelf(const NetworkIdentity &ID)
{
	Log logger("ServerRegistry");
	logger.DebugFormatted("DeRegisterSelf: {}", ID.ToString());
	const std::string key = GetKeyOfIdentifier(ID);
	InternalDB::Get()->HDel(HashTableNameID_IP, {key});
	InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix, {key});
}

const decltype(ServerRegistry::servers) &ServerRegistry::GetServers()
{
	Log logger("ServerRegistry");
	servers.clear();
	const auto entries = InternalDB::Get()->HGetAll(HashTableNameID_IP);
	logger.DebugFormatted("GetServers: {} entries", entries.size());
	for (const auto &rawEntry : entries)
	{
		ServerRegistryEntry newEntry;
		NetworkIdentity id;
		ByteReader br(rawEntry.first);
		id.Deserialize(br);
		
		newEntry.identifier = id;
		newEntry.address.Parse(rawEntry.second);

		if (newEntry.identifier.Type == NetworkIdentityType::eInvalid)
		{
			logger.Error("GetServers: invalid identifier type encountered; skipping entry");
			continue;
		}
		if (servers.contains(newEntry.identifier))
		{
			logger.ErrorFormatted("GetServers: duplicate entry for {}", newEntry.identifier.ToString());
			continue;
		}
		servers.insert(std::make_pair(newEntry.identifier, newEntry));
	}
	return servers;
}

std::optional<IPAddress> ServerRegistry::GetIPOfID(const NetworkIdentity &ID)
{
	Log logger("ServerRegistry");
	logger.DebugFormatted("GetIPOfID: {}", ID.ToString());
	const auto ret =
		InternalDB::Get()->HGet(HashTableNameID_IP, GetKeyOfIdentifier(ID));
	if (!ret.has_value() || ret->empty())
	{
		logger.WarningFormatted("GetIPOfID: no entry found for {}", ID.ToString());
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(*ret);
	return ip;
}

std::optional<IPAddress> ServerRegistry::GetPublicAddress(const NetworkIdentity &ID)
{
	Log logger("ServerRegistry");
	logger.DebugFormatted("GetPublicAddress: {}", ID.ToString());
	const auto ret = InternalDB::Get()->HGet(HashTableNameID_IP + kPublicSuffix,
	                                         GetKeyOfIdentifier(ID));
	if (!ret.has_value() || ret->empty())
	{
		logger.WarningFormatted("GetPublicAddress: no public entry found for {}", ID.ToString());
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(*ret);
	return ip;
}

bool ServerRegistry::ExistsInRegistry(const NetworkIdentity &ID) const
{
	Log logger("ServerRegistry");
	ByteWriter bw;
	ID.Serialize(bw);
	const bool exists = InternalDB::Get()->HExists(HashTableNameID_IP, GetKeyOfIdentifier(ID));
	logger.DebugFormatted("ExistsInRegistry: {} -> {}", ID.ToString(), exists ? "true" : "false");
	return exists;
}

void ServerRegistry::ClearAll()
{
	Log logger("ServerRegistry");
	logger.Debug("ClearAll called; deleting all ServerRegistry keys");
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


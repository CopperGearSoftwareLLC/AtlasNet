#include "ServerRegistry.hpp"

#include "InternalDB.hpp"
#include "Misc/String_utils.hpp"

static const std::string kPublicSuffix = "_public";

void ServerRegistry::RegisterSelf(const InterLinkIdentifier &ID, IPAddress address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP, NukeString(ID.ToString()),
							NukeString(address.ToString()));
}

void ServerRegistry::RegisterPublicAddress(const InterLinkIdentifier &ID, const IPAddress &address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP + kPublicSuffix, NukeString(ID.ToString()),
							NukeString(address.ToString()));
}

void ServerRegistry::DeRegisterSelf(const InterLinkIdentifier &ID)
{
	InternalDB::Get()->HDel(HashTableNameID_IP, {NukeString(ID.ToString())});
	InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix, {NukeString(ID.ToString())});
}

const decltype(ServerRegistry::servers) &ServerRegistry::GetServers()
{
	servers.clear();
	const auto entries = InternalDB::Get()->HGetAll(HashTableNameID_IP);
	for (const auto &rawEntry : entries)
	{
		ServerRegistryEntry newEntry;
		auto Type = InterLinkIdentifier::FromString(NukeString(rawEntry.first));
		if (!Type.has_value())
		{
			std::cerr << "Unable to parse " << rawEntry.first << " " << rawEntry.second
					  << std::endl;
			continue;
		}
		newEntry.identifier = Type.value();
		newEntry.address.Parse(rawEntry.second);

		ASSERT(newEntry.identifier.Type != InterlinkType::eInvalid,
			   "Invalid entry in server ServerRegistry");
		ASSERT(!servers.contains(newEntry.identifier), "Duplicate entry in server ServerRegistry?");
		servers.insert(std::make_pair(newEntry.identifier, newEntry));
	}
	return servers;
}

std::optional<IPAddress> ServerRegistry::GetIPOfID(const InterLinkIdentifier &ID)
{
	std::string ret =
		InternalDB::Get()->HGet(HashTableNameID_IP, NukeString(ID.ToString())).value();
	if (ret.empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(ret);
	return ip;
}

std::optional<IPAddress> ServerRegistry::GetPublicAddress(const InterLinkIdentifier &ID)
{
	std::string ret = InternalDB::Get()
						  ->HGet(HashTableNameID_IP + kPublicSuffix, NukeString(ID.ToString()))
						  .value();
	if (ret.empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(ret);
	return ip;
}

bool ServerRegistry::ExistsInRegistry(const InterLinkIdentifier &ID) const
{
	return InternalDB::Get()->HExists(HashTableNameID_IP, NukeString(ID.ToString()));
}

void ServerRegistry::ClearAll()
{
	InternalDB::Get()->DelKey(HashTableNameID_IP);
	InternalDB::Get()->DelKey(HashTableNameID_IP + kPublicSuffix);
}

ServerRegistry::ServerRegistry() {}

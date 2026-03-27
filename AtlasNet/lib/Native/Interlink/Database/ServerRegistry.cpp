#include "ServerRegistry.hpp"

#include "Network/NetworkIdentity.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Global/Misc/String_utils.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"

static const std::string kPublicSuffix = "_public";
static const std::string kRegistryTableName = "Server Registry ID_IP";
static const std::string kLastSeenSuffix = "_last_seen";

namespace
{
constexpr double kRegistryHealthGraceSeconds = 1.0;

std::vector<std::string_view> MakeStringViews(const std::vector<std::string>& values)
{
	std::vector<std::string_view> views;
	views.reserve(values.size());
	for (const auto& value : values)
	{
		views.emplace_back(value);
	}
	return views;
}

void TouchRegistryLastSeen(const NetworkIdentity& identifier)
{
	ByteWriter bw;
	identifier.Serialize(bw);
	InternalDB::Get()->HSet(kRegistryTableName + kLastSeenSuffix,
							std::string(bw.as_string_view()),
							std::to_string(InternalDB::Get()->GetTimeNowSeconds()));
}

bool ShouldKeepRegistryEntry(const NetworkIdentity& identifier,
							 const std::unordered_map<std::string, double>& healthPings,
							 const std::unordered_map<std::string, double>& lastSeenByIdentifier,
							 const double nowSeconds)
{
	if (identifier.Type == NetworkIdentityType::eCartograph)
	{
		return true;
	}

	ByteWriter bw;
	identifier.Serialize(bw);
	const std::string identifierKey(bw.as_string_view());
	const auto pingIt = healthPings.find(identifierKey);
	if (pingIt != healthPings.end() && pingIt->second > nowSeconds)
	{
		return true;
	}

	const auto lastSeenIt = lastSeenByIdentifier.find(identifierKey);
	return lastSeenIt != lastSeenByIdentifier.end() &&
		   (nowSeconds - lastSeenIt->second) <= kRegistryHealthGraceSeconds;
}

void CollectStaleRegistryKeys(const std::string& tableName,
							  const std::unordered_map<std::string, double>& healthPings,
							  const std::unordered_map<std::string, double>& lastSeenByIdentifier,
							  const double nowSeconds,
							  std::vector<std::string>& outKeys)
{
	const auto entries = InternalDB::Get()->HGetAll(tableName);
	outKeys.reserve(outKeys.size() + entries.size());

	for (const auto& rawEntry : entries)
	{
		try
		{
			NetworkIdentity identifier;
			ByteReader br(rawEntry.first);
			identifier.Deserialize(br);
			if (!ShouldKeepRegistryEntry(identifier, healthPings, lastSeenByIdentifier, nowSeconds))
			{
				outKeys.push_back(rawEntry.first);
			}
		}
		catch (...)
		{
			outKeys.push_back(rawEntry.first);
		}
	}
}
}  // namespace

void ServerRegistry::RegisterSelf(const NetworkIdentity &ID, IPAddress address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP, GetKeyOfIdentifier(ID),
								NukeString(address.ToString()));
	TouchRegistryLastSeen(ID);
}

void ServerRegistry::RegisterPublicAddress(const NetworkIdentity &ID, const IPAddress &address)
{
	InternalDB::Get()->HSet(HashTableNameID_IP + kPublicSuffix, GetKeyOfIdentifier(ID),
								NukeString(address.ToString()));
	TouchRegistryLastSeen(ID);
}

void ServerRegistry::DeRegisterSelf(const NetworkIdentity &ID)
{
	const std::string key = GetKeyOfIdentifier(ID);
	InternalDB::Get()->HDel(HashTableNameID_IP, {key});
	InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix, {key});
	InternalDB::Get()->HDel(HashTableNameID_IP + kLastSeenSuffix, {key});
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
	const auto ret =
		InternalDB::Get()->HGet(HashTableNameID_IP, GetKeyOfIdentifier(ID));
	if (!ret.has_value() || ret->empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(*ret);
	return ip;
}

std::optional<IPAddress> ServerRegistry::GetPublicAddress(const NetworkIdentity &ID)
{
	const auto ret = InternalDB::Get()->HGet(HashTableNameID_IP + kPublicSuffix,
	                                         GetKeyOfIdentifier(ID));
	if (!ret.has_value() || ret->empty())
	{
		return std::nullopt;
	}
	IPAddress ip;
	ip.Parse(*ret);
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
	InternalDB::Get()->DelKey(HashTableNameID_IP + kLastSeenSuffix);
}

size_t ServerRegistry::PruneEntriesMissingHealthPings(
	const std::unordered_map<std::string, double>& healthPings, double nowSeconds)
{
	std::vector<std::string> stalePrivateKeys;
	std::vector<std::string> stalePublicKeys;
	std::unordered_map<std::string, double> lastSeenByIdentifier;
	const auto lastSeenEntries = InternalDB::Get()->HGetAll(HashTableNameID_IP + kLastSeenSuffix);
	lastSeenByIdentifier.reserve(lastSeenEntries.size());
	for (const auto& [key, value] : lastSeenEntries)
	{
		try
		{
			lastSeenByIdentifier.emplace(key, std::stod(value));
		}
		catch (...)
		{
		}
	}

	CollectStaleRegistryKeys(HashTableNameID_IP, healthPings, lastSeenByIdentifier, nowSeconds,
							 stalePrivateKeys);
	CollectStaleRegistryKeys(HashTableNameID_IP + kPublicSuffix, healthPings, lastSeenByIdentifier,
							 nowSeconds,
							 stalePublicKeys);

	if (!stalePrivateKeys.empty())
	{
		const auto stalePrivateViews = MakeStringViews(stalePrivateKeys);
		InternalDB::Get()->HDel(HashTableNameID_IP, stalePrivateViews);
	}
	if (!stalePublicKeys.empty())
	{
		const auto stalePublicViews = MakeStringViews(stalePublicKeys);
		InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix, stalePublicViews);
	}
	if (!stalePrivateKeys.empty() || !stalePublicKeys.empty())
	{
		std::vector<std::string> staleLastSeenKeys = stalePrivateKeys;
		staleLastSeenKeys.insert(staleLastSeenKeys.end(), stalePublicKeys.begin(),
								 stalePublicKeys.end());
		std::sort(staleLastSeenKeys.begin(), staleLastSeenKeys.end());
		staleLastSeenKeys.erase(std::unique(staleLastSeenKeys.begin(), staleLastSeenKeys.end()),
								staleLastSeenKeys.end());
		const auto staleLastSeenViews = MakeStringViews(staleLastSeenKeys);
		InternalDB::Get()->HDel(HashTableNameID_IP + kLastSeenSuffix, staleLastSeenViews);
	}

	return stalePrivateKeys.size() + stalePublicKeys.size();
}

ServerRegistry::ServerRegistry() {}
const std::string ServerRegistry::GetKeyOfIdentifier(const NetworkIdentity &ID) {
			ByteWriter bw;
		ID.Serialize(bw);
	std::string s(bw.as_string_view());
	return s;
}

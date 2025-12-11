#include "ServerRegistry.hpp"
#include "misc/String_utils.hpp"

static const std::string kPublicSuffix = "_public";

void ServerRegistry::RegisterSelf(const InterLinkIdentifier &ID, IPAddress address)
{
    database->HashSet(HashTableNameID_IP, NukeString(ID.ToString()), NukeString(address.ToString()));
}

void ServerRegistry::RegisterPublicAddress(const InterLinkIdentifier &ID, const IPAddress &address)
{
    database->HashSet(HashTableNameID_IP + kPublicSuffix, NukeString(ID.ToString()), NukeString(address.ToString()));
}

void ServerRegistry::DeRegisterSelf(const InterLinkIdentifier &ID)
{
    database->HashRemove(HashTableNameID_IP, NukeString(ID.ToString()));
    database->HashRemove(HashTableNameID_IP + kPublicSuffix, NukeString(ID.ToString()));
}

const decltype(ServerRegistry::servers) &ServerRegistry::GetServers()
{
    servers.clear();

    const auto ServerEntriesRaw = database->HashGetAll(HashTableNameID_IP);
    for (const auto &rawEntry : ServerEntriesRaw)
    {
        ServerRegistryEntry newEntry;
        auto Type = InterLinkIdentifier::FromString(NukeString(rawEntry.first));
        if (!Type.has_value())
        {
            std::cerr << "Unable to parse " << rawEntry.first << " " << rawEntry.second << std::endl;
            continue;
        }
        newEntry.identifier = Type.value();
        newEntry.address.Parse(rawEntry.second);

        ASSERT(newEntry.identifier.Type != InterlinkType::eInvalid, "Invalid entry in server ServerRegistry");
        ASSERT(!servers.contains(newEntry.identifier), "Duplicate entry in server ServerRegistry?");
        servers.insert(std::make_pair(newEntry.identifier, newEntry));
    }
    return servers;
}

std::optional<IPAddress> ServerRegistry::GetIPOfID(const InterLinkIdentifier &ID)
{
  std::string ret = database->HashGet(HashTableNameID_IP, NukeString(ID.ToString()));
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
    std::string ret = database->HashGet(HashTableNameID_IP + kPublicSuffix, NukeString(ID.ToString()));
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
    return database->HashExists(HashTableNameID_IP, NukeString(ID.ToString()));
}

void ServerRegistry::ClearAll()
{
    database->HashRemoveAll(HashTableNameID_IP);
    database->HashRemoveAll(HashTableNameID_IP + kPublicSuffix);
}

ServerRegistry::ServerRegistry()
{
#ifdef _LOCAL
    // Start/Connect to a local Redis instance for standalone runs
    database = new RedisCacheDatabase(true, "127.0.0.1", 6379, "");
#else
    database = new RedisCacheDatabase();
#endif
    if (!database->Connect())
    {
        throw std::runtime_error("unale to connect to da database");
    }
}

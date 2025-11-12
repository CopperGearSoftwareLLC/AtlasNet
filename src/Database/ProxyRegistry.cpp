#include "ProxyRegistry.hpp"
#include "misc/String_utils.hpp"

static const std::string kPublicSuffix = "_public";

ProxyRegistry::ProxyRegistry()
{
    database = std::make_unique<RedisCacheDatabase>();
    if (!database->Connect())
        throw std::runtime_error("Unable to connect to Redis in ProxyRegistry");
}

void ProxyRegistry::RegisterSelf(const InterLinkIdentifier& id, const IPAddress& address)
{
    database->HashSet(HashTableNameID_IP, NukeString(id.ToString()), NukeString(address.ToString()));
}

void ProxyRegistry::RegisterPublicAddress(const InterLinkIdentifier& id, const IPAddress& address)
{
    database->HashSet(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()), NukeString(address.ToString()));
}

void ProxyRegistry::DeRegisterSelf(const InterLinkIdentifier& id)
{
    database->HashRemove(HashTableNameID_IP, NukeString(id.ToString()));
    database->HashRemove(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()));
}

std::optional<IPAddress> ProxyRegistry::GetIPOfID(const InterLinkIdentifier& id)
{
    std::string ret = database->HashGet(HashTableNameID_IP, NukeString(id.ToString()));
    if (ret.empty())
        return std::nullopt;

    IPAddress ip;
    ip.Parse(ret);
    return ip;
}

std::optional<IPAddress> ProxyRegistry::GetPublicAddress(const InterLinkIdentifier& id)
{
    std::string ret = database->HashGet(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()));
    if (ret.empty())
        return std::nullopt;

    IPAddress ip;
    ip.Parse(ret);
    return ip;
}

bool ProxyRegistry::ExistsInRegistry(const InterLinkIdentifier& id) const
{
    return database->HashExists(HashTableNameID_IP, NukeString(id.ToString()));
}

const std::unordered_map<InterLinkIdentifier, ProxyRegistry::ProxyRegistryEntry>& ProxyRegistry::GetProxies()
{
    proxies.clear();
    const auto ProxyEntriesRaw = database->HashGetAll(HashTableNameID_IP);
    for (const auto& raw : ProxyEntriesRaw)
    {
        ProxyRegistryEntry entry;
        auto maybeID = InterLinkIdentifier::FromString(NukeString(raw.first));
        if (!maybeID.has_value())
        {
            std::cerr << "ProxyRegistry: failed to parse identifier " << raw.first << std::endl;
            continue;
        }

        entry.identifier = maybeID.value();
        entry.address.Parse(raw.second);

        proxies.insert({ entry.identifier, entry });
    }
    return proxies;
}

void ProxyRegistry::ClearAll()
{
    database->HashRemoveAll(HashTableNameID_IP);
    database->HashRemoveAll(HashTableNameID_IP + kPublicSuffix);
}

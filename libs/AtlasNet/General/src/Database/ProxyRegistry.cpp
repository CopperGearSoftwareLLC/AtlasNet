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
    // initialize load to 0
    database->HashSet(HashTableNameID_Load, NukeString(id.ToString()), "0");
}

void ProxyRegistry::RegisterPublicAddress(const InterLinkIdentifier& id, const IPAddress& address)
{
    database->HashSet(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()), NukeString(address.ToString()));
}

void ProxyRegistry::DeRegisterSelf(const InterLinkIdentifier& id)
{
    database->HashRemove(HashTableNameID_IP, NukeString(id.ToString()));
    database->HashRemove(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()));
    database->HashRemove(HashTableNameID_Load, NukeString(id.ToString()));
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

/*-------------------------------------------------------------
 * LOAD TRACKING
 *------------------------------------------------------------*/
void ProxyRegistry::IncrementClient(const InterLinkIdentifier& proxyID)
{
    std::string key = NukeString(proxyID.ToString());
    uint32_t load = std::stoi(database->HashGet(HashTableNameID_Load, key));
    load++;
    database->HashSet(HashTableNameID_Load, key, std::to_string(load));
}

void ProxyRegistry::DecrementClient(const InterLinkIdentifier& proxyID)
{
    std::string key = NukeString(proxyID.ToString());
    uint32_t load = std::stoi(database->HashGet(HashTableNameID_Load, key));
    if (load > 0) load--;
    database->HashSet(HashTableNameID_Load, key, std::to_string(load));
}

uint32_t ProxyRegistry::GetLoad(const InterLinkIdentifier& proxyID)
{
    std::string ret = database->HashGet(HashTableNameID_Load, NukeString(proxyID.ToString()));
    if (ret.empty())
        return 0;
    return std::stoul(ret);
}

/*-------------------------------------------------------------
 * CLIENT to PROXY OWNERSHIP
 *------------------------------------------------------------*/

void ProxyRegistry::AssignClientToProxy(const std::string& clientID,
                                        const InterLinkIdentifier& proxyID)
{
    database->HashSet(HashTableNameClientOwner,
                      NukeString(clientID),
                      NukeString(proxyID.ToString()));

    IncrementClient(proxyID);
}

std::optional<InterLinkIdentifier> ProxyRegistry::GetProxyOfClient(const std::string& clientID)
{
    std::string ret = database->HashGet(HashTableNameClientOwner, NukeString(clientID));
    if (ret.empty())
        return std::nullopt;

    auto maybeID = InterLinkIdentifier::FromString(ret);
    if (!maybeID.has_value())
        return std::nullopt;

    return maybeID;
}


/*-------------------------------------------------------------
 * PROXY ENUMERATION
 *------------------------------------------------------------*/

const std::unordered_map<InterLinkIdentifier, ProxyRegistry::ProxyRegistryEntry>& 
ProxyRegistry::GetProxies()
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

        // Load from Redis
        std::string loadStr = database->HashGet(HashTableNameID_Load, raw.first);
        entry.load = loadStr.empty() ? 0 : std::stoul(loadStr);

        proxies.insert({ entry.identifier, entry });
    }
    return proxies;
}

std::optional<InterLinkIdentifier> ProxyRegistry::GetLeastLoadedProxy()
{
    GetProxies(); // refresh cache

    if (proxies.empty())
        return std::nullopt;

    auto min_it = std::min_element(
        proxies.begin(), proxies.end(),
        [](const auto& a, const auto& b) {
            return a.second.load < b.second.load;
        });

    if (min_it == proxies.end())
        return std::nullopt;

    return min_it->second.identifier;
}

/*-------------------------------------------------------------
 * UTILITY
 *------------------------------------------------------------*/

bool ProxyRegistry::ExistsInRegistry(const InterLinkIdentifier& id) const
{
    return database->HashExists(HashTableNameID_IP, NukeString(id.ToString()));
}

void ProxyRegistry::ClearAll()
{
    database->HashRemoveAll(HashTableNameID_IP);
    database->HashRemoveAll(HashTableNameID_IP + kPublicSuffix);
    database->HashRemoveAll(HashTableNameID_Load);
    database->HashRemoveAll(HashTableNameClientOwner);
}
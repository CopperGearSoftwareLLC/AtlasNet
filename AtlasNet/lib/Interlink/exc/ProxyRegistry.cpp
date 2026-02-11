#include "ProxyRegistry.hpp"
#include "InterlinkIdentifier.hpp"
#include "InternalDB.hpp"
#include "Misc/String_utils.hpp"
#include "Serialize/ByteReader.hpp"
#include <iostream>
static const std::string kPublicSuffix = "_public";

ProxyRegistry::ProxyRegistry()
{
   
}

void ProxyRegistry::RegisterSelf(const InterLinkIdentifier& id, const IPAddress& address)
{
    
    InternalDB::Get()->HSet(HashTableNameID_IP, NukeString(id.ToString()), NukeString(address.ToString()));
    // initialize load to 0
    InternalDB::Get()->HSet(HashTableNameID_Load, NukeString(id.ToString()), "0");
}

void ProxyRegistry::RegisterPublicAddress(const InterLinkIdentifier& id, const IPAddress& address)
{
    InternalDB::Get()->HSet(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString()), NukeString(address.ToString()));
}

void ProxyRegistry::DeRegisterSelf(const InterLinkIdentifier& id)
{
    InternalDB::Get()->HDel(HashTableNameID_IP, {NukeString(id.ToString())});
    InternalDB::Get()->HDel(HashTableNameID_IP + kPublicSuffix,{ NukeString(id.ToString())});
    InternalDB::Get()->HDel(HashTableNameID_Load, {NukeString(id.ToString())});
}

std::optional<IPAddress> ProxyRegistry::GetIPOfID(const InterLinkIdentifier& id)
{
    std::string ret = InternalDB::Get()->HGet(HashTableNameID_IP, NukeString(id.ToString())).value();
    if (ret.empty())
        return std::nullopt;

    IPAddress ip;
    ip.Parse(ret);
    return ip;
}

std::optional<IPAddress> ProxyRegistry::GetPublicAddress(const InterLinkIdentifier& id)
{
    std::string ret = InternalDB::Get()->HGet(HashTableNameID_IP + kPublicSuffix, NukeString(id.ToString())).value();
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
    uint32_t load = std::stoi(InternalDB::Get()->HGet(HashTableNameID_Load, key).value());
    load++;
    InternalDB::Get()->HSet(HashTableNameID_Load, key, std::to_string(load));
}

void ProxyRegistry::DecrementClient(const InterLinkIdentifier& proxyID)
{
    std::string key = NukeString(proxyID.ToString());
    uint32_t load = std::stoi(InternalDB::Get()->HGet(HashTableNameID_Load, key).value());
    if (load > 0) load--;
    InternalDB::Get()->HSet(HashTableNameID_Load, key, std::to_string(load));
}

uint32_t ProxyRegistry::GetLoad(const InterLinkIdentifier& proxyID)
{
    std::string ret = InternalDB::Get()->HGet(HashTableNameID_Load, NukeString(proxyID.ToString())).value();
    if (ret.empty())
        return 0;
    return std::stoul(ret);
}

/*-------------------------------------------------------------
 * CLIENT to PROXY OWNERSHIP
 *------------------------------------------------------------*/
/*
void ProxyRegistry::AssignClientToProxy(const std::string& clientID,
                                        const InterLinkIdentifier& proxyID)
{
    InternalDB::Get()->HSet(HashTableNameClientOwner,
                      NukeString(clientID),
                      NukeString(proxyID.ToString()));

    IncrementClient(proxyID);
}

std::optional<InterLinkIdentifier> ProxyRegistry::GetProxyOfClient(const std::string& clientID)
{
    const auto key = GetKeyOfIdentifier(id);
    std::string ret = InternalDB::Get()->HGet(HashTableNameClientOwner, key).value();
    if (ret.empty())
        return std::nullopt;

        InterLinkIdentifier id;
        id.Deserialize(ByteReader(ret));
    auto maybeID = InterLinkIdentifier::FromString(ret);
    if (!maybeID.has_value())
        return std::nullopt;

    return maybeID;
}
*/

/*-------------------------------------------------------------
 * PROXY ENUMERATION
 *------------------------------------------------------------*/

const std::unordered_map<InterLinkIdentifier, ProxyRegistry::ProxyRegistryEntry>& 
ProxyRegistry::GetProxies()
{
    proxies.clear();
    const auto ProxyEntriesRaw = InternalDB::Get()->HGetAll(HashTableNameID_IP);

    for (const auto& raw : ProxyEntriesRaw)
    {
        ProxyRegistryEntry entry;

        ByteReader br(raw.first);
        entry.identifier.Deserialize(br);
        entry.address.Parse(raw.second);

        // Load from Redis
        std::string loadStr = InternalDB::Get()->HGet(HashTableNameID_Load, raw.first).value();
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
    return InternalDB::Get()->HExists(HashTableNameID_IP, GetKeyOfIdentifier(id));
}

void ProxyRegistry::ClearAll()
{
    InternalDB::Get()->DelKey(HashTableNameID_IP);
    InternalDB::Get()->DelKey(HashTableNameID_IP + kPublicSuffix);
    InternalDB::Get()->DelKey(HashTableNameID_Load);
    InternalDB::Get()->DelKey(HashTableNameClientOwner);
}
const std::string ProxyRegistry::GetKeyOfIdentifier(const InterLinkIdentifier& ID){
		ByteWriter bw;
	ID.Serialize(bw);
	std::string s(bw.as_string_view());
	return s;
}

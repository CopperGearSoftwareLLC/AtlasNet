#include "ServerRegistry.hpp"
#include "misc/String_utils.hpp"
void ServerRegistry::RegisterSelf(const InterLinkIdentifier &ID, IPAddress address)
{
    database->HashSet(HashTableNameID_IP, NukeString(ID.ToString()), NukeString(address.ToString()));
    // database->HashSet(HashTableNameIP_ID, address.ToString(), ID.ToString());
}

void ServerRegistry::DeRegisterSelf(const InterLinkIdentifier &ID)
{
    database->HashRemove(HashTableNameID_IP, NukeString(ID.ToString()));
    // database->HashRemove(HashTableNameIP_ID, ID.ToString());
}

const decltype(ServerRegistry::servers) &ServerRegistry::GetServers()
{
    servers.clear();
    const auto ServerEntriesRaw = database->HashGetAll(HashTableNameID_IP);
    for (const auto &rawEntry : ServerEntriesRaw)
    {
        ServerRegistryEntry newEntry;
        InterLinkIdentifier ID;
        auto Type = InterLinkIdentifier::FromString(NukeString(rawEntry.first));
        if (!Type.has_value())
        {
            std::cerr << "Unable to parse " << rawEntry.first << " " << rawEntry.second << std::endl;
            continue;
        }
        ID = Type.value();
        newEntry.identifier = ID;
        newEntry.address.Parse(rawEntry.second);
        ASSERT(newEntry.identifier.Type != InterlinkType::eInvalid, "Invalid entry in server ServerRegistry");
        ASSERT(!servers.contains(ID), "Duplicate entry in server ServerRegistry?");
        servers.insert(std::make_pair(ID, newEntry));
    }
    return servers;
}

std::optional<IPAddress> ServerRegistry::GetIPOfID(const InterLinkIdentifier &ID)
{
    if (std::string ret = database->HashGet(HashTableNameID_IP, NukeString(ID.ToString())); ret.empty())
    {
        
        return std::nullopt;
    }
    else
    {
        IPAddress ip;
        ip.Parse(ret);
        return ip;
    }
}
bool ServerRegistry::ExistsInRegistry(const InterLinkIdentifier &ID) const
{
    return database->HashExists(HashTableNameID_IP, NukeString(ID.ToString()));
}
void ServerRegistry::ClearAll()
{
    database->HashRemoveAll(HashTableNameID_IP);
}
/*
std::optional<InterLinkIdentifier> ServerRegistry::GetIDOfIP(IPAddress IP, bool IgnorePort)
{
    if (!IgnorePort)
    {
        if (std::string ret = database->HashGet(HashTableNameIP_ID, IP.ToString()); ret.empty())
        {
            return std::nullopt;
        }
        else
        {
            return InterLinkIdentifier::FromString(ret);
        }
    }
    else
    {
        const auto AllEntries = database->HashGetAll(HashTableNameIP_ID);
        for (const auto &[key, value] : AllEntries)
        {
            auto remove_ws = [](std::string s)
            {
                std::erase_if(s, [](unsigned char c)
                              { return std::isspace(c); });
                return s;
            };
            // key is like "192.168.1.5:25565"
            auto pos = key.find(':');
            std::string ip = remove_ws(pos == std::string::npos ? key : key.substr(0, pos));
            std::string CompareAgainst = remove_ws(IP.ToString(false));
            ip.shrink_to_fit();
            CompareAgainst.shrink_to_fit();
            size_t n = std::min(ip.length(), CompareAgainst.size());
            if (ip.compare(0, n, CompareAgainst, 0, n) == 0)
            {
                return InterLinkIdentifier::FromString(value); // found a match
            }
        }
        return std::nullopt;
    }
}
*/
ServerRegistry::ServerRegistry()
{
    database = new RedisCacheDatabase();
    if (!database->Connect())
    {
        throw std::runtime_error("unale to connect to da database");
    }
}

#pragma once
#include <pch.hpp>
#include <Interlink/Interlink.hpp>
#include "Database/RedisCacheDatabase.hpp"
#include "Interlink/InterlinkIdentifier.hpp"
struct ServerRegistryEntry
{
    IPAddress address;
    InterLinkIdentifier identifier;
};
class ServerRegistry:public Singleton<ServerRegistry>
{
IDatabase* database;
    std::unordered_map<InterLinkIdentifier,ServerRegistryEntry> servers;
    const static inline std::string HashTableNameID_IP= "Server Registry ID_IP";
    //const static inline std::string HashTableNameIP_ID = "Server Registry IP_ID";

    public:
    ServerRegistry();

    void RegisterSelf(const InterLinkIdentifier& ID, IPAddress address);
    void DeRegisterSelf(const InterLinkIdentifier& ID);
    const decltype(servers)& GetServers();
    std::optional<IPAddress> GetIPOfID(const InterLinkIdentifier& ID);
    bool ExistsInRegistry(const InterLinkIdentifier& ID) const;
    void ClearAll();
    //std::optional<InterLinkIdentifier> GetIDOfIP(IPAddress ID,bool IgnorePort);
    void RegisterPublicAddress(const InterLinkIdentifier& ID, const IPAddress& address);
    std::optional<IPAddress> GetPublicAddress(const InterLinkIdentifier& ID);
};
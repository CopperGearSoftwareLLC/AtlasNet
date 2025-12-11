#pragma once
#include "Database/RedisCacheDatabase.hpp"
#include "Interlink/InterlinkIdentifier.hpp"
#include "Interlink/Connection.hpp"
#include <unordered_map>
#include <optional>
#include <string>
#include <memory>

/**
 * @brief Registry for Demigod proxy servers.
 *        Mirrors the structure of ServerRegistry but uses a distinct Redis hash table.
 *        Used by GameCoordinator to discover and assign proxies for incoming clients.
 */
class ProxyRegistry
{
public:
    struct ProxyRegistryEntry
    {
        InterLinkIdentifier identifier;
        IPAddress address;
        uint32_t load = 0; // Number of connected clients
    };

    static ProxyRegistry& Get()
    {
        static ProxyRegistry instance;
        return instance;
    }

    // Registration
    void RegisterSelf(const InterLinkIdentifier& id, const IPAddress& address);
    void RegisterPublicAddress(const InterLinkIdentifier& id, const IPAddress& address);
    void DeRegisterSelf(const InterLinkIdentifier& id);

    std::optional<IPAddress> GetIPOfID(const InterLinkIdentifier& id);
    std::optional<IPAddress> GetPublicAddress(const InterLinkIdentifier& id);
    // Utility
    bool ExistsInRegistry(const InterLinkIdentifier& id) const;
    void ClearAll();

    // Load tracking
    void IncrementClient(const InterLinkIdentifier& id);
    void DecrementClient(const InterLinkIdentifier& id);
    uint32_t GetLoad(const InterLinkIdentifier& id);

    // Client to Proxy ownership. implicitly increments client load
    void AssignClientToProxy(const std::string& clientID, const InterLinkIdentifier& proxyID);
    std::optional<InterLinkIdentifier> GetProxyOfClient(const std::string& clientID);

    // Proxy enumeration
    const std::unordered_map<InterLinkIdentifier, ProxyRegistryEntry>& GetProxies();
    std::optional<InterLinkIdentifier> GetLeastLoadedProxy();


private:
    ProxyRegistry();

private:
    std::unique_ptr<RedisCacheDatabase> database;
    // Cached proxy entries
    std::unordered_map<InterLinkIdentifier, ProxyRegistryEntry> proxies;
    // Redis table names
    const std::string HashTableNameID_IP = "ProxyRegistry_ID_IP";
    const std::string HashTableNameID_Load = "ProxyRegistry_ID_Load";
    const std::string HashTableNameClientOwner = "ProxyRegistry_ClientOwner";
};

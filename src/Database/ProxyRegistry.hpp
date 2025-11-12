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
    };

    static ProxyRegistry& Get()
    {
        static ProxyRegistry instance;
        return instance;
    }

    void RegisterSelf(const InterLinkIdentifier& id, const IPAddress& address);
    void RegisterPublicAddress(const InterLinkIdentifier& id, const IPAddress& address);
    void DeRegisterSelf(const InterLinkIdentifier& id);

    std::optional<IPAddress> GetIPOfID(const InterLinkIdentifier& id);
    std::optional<IPAddress> GetPublicAddress(const InterLinkIdentifier& id);
    bool ExistsInRegistry(const InterLinkIdentifier& id) const;

    const std::unordered_map<InterLinkIdentifier, ProxyRegistryEntry>& GetProxies();
    void ClearAll();

private:
    ProxyRegistry();

private:
    std::unique_ptr<RedisCacheDatabase> database;
    std::unordered_map<InterLinkIdentifier, ProxyRegistryEntry> proxies;
    const std::string HashTableNameID_IP = "ProxyRegistry_ID_IP";
};

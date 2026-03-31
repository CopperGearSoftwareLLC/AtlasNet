#pragma once
#include <Global/pch.hpp>
#include <Interlink/Interlink.hpp>
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"
struct ServerRegistryEntry
{
    IPAddress address;
    NetworkIdentity identifier;
};
class ServerRegistry:public Singleton<ServerRegistry>
{

    std::unordered_map<NetworkIdentity,ServerRegistryEntry> servers;
    const static inline std::string HashTableNameID_IP= "Server Registry ID_IP";
    //const static inline std::string HashTableNameIP_ID = "Server Registry IP_ID";
	const static std::string GetKeyOfIdentifier(const NetworkIdentity& ID);

   public:
    ServerRegistry();

    void RegisterSelf(const NetworkIdentity& ID, IPAddress address);
    void DeRegisterSelf(const NetworkIdentity& ID);
    const decltype(servers)& GetServers();
    std::optional<IPAddress> GetIPOfID(const NetworkIdentity& ID);
    bool ExistsInRegistry(const NetworkIdentity& ID) const;
    void ClearAll();
    //std::optional<NetworkIdentity> GetIDOfIP(IPAddress ID,bool IgnorePort);
    void RegisterPublicAddress(const NetworkIdentity& ID, const IPAddress& address);
    std::optional<IPAddress> GetPublicAddress(const NetworkIdentity& ID);

};
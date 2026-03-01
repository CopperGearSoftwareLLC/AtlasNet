#pragma once

#include <unordered_map>

#include "Client/Client.hpp"
#include "Client/Packet/ClientSpawnPacket.hpp"
#include "Debug/Log.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/Packet/PacketManager.hpp"
class ClientLedger : public Singleton<ClientLedger>
{
	Log logger = Log("ClientLedger");
	std::unordered_map<ClientID, Client> Clients;

	PacketManager::Subscription sub_ClientSpawnpacket;

   public:
	ClientLedger()
	{
		sub_ClientSpawnpacket = Interlink::Get().GetPacketManager().Subscribe<ClientSpawnPacket>(
			[this](const auto& p, const auto& i) { OnClientSpawnPacket(p, i); });
	}

   private:
	void OnClientSpawnPacket(const ClientSpawnPacket& p, const PacketManager::PacketInfo& info)
	{
		ASSERT(p.stage == ClientSpawnPacket::Stage::eNotification, "Invalid packet");
		for (const auto& c : p.GetAsNotification().incomingClients)
		{
			logger.DebugFormatted("Spawning Client {} at {}", UUIDGen::ToString(c.client.ID),
								  c.spawn_Location.ToString());
			ClientConnectEvent ev;
            ev.client = c.client;
            ev.ConnectedProxy = info.sender;
            ev.SpawnLocation = c.spawn_Location;
            ev.ConnectedShard = NetworkCredentials::Get().GetID();
			EventSystem::Get().Dispatch(ev);
		}
	}
};
#pragma once

#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

#include <iostream>
#include <thread>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/Connection.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Client/ClientIDAssignPacket.hpp"
#include "Network/Packet/PacketManager.hpp"

class ClientLink : public Singleton<ClientLink>
{
   public:
	void Init();
	void ConnectToAtlasNet(const IPAddress& address);

	void OnSteamNetConnectionStatusChanged(
		SteamNetConnectionStatusChangedCallback_t* pInfo);

	PacketManager& GetPacketManager() { return packet_manager; }

   private:
	void OnConnected(SteamNetConnectionStatusChangedCallback_t* pInfo);
	std::jthread TickThread;
	void Update();

	HSteamNetPollGroup poll_group;

	void InitGNS();

	void ReceiveMessages();

	void OnClientIDAssignedPacket(const ClientIDAssignPacket& clientIDPacket,const PacketManager::PacketInfo&);

	PacketManager packet_manager;
	
	PacketManager::Subscription ClientIDAssignSub =
		packet_manager.Subscribe<ClientIDAssignPacket>(
			[&](const ClientIDAssignPacket& clientIDPacket,const PacketManager::PacketInfo& info)
			{ OnClientIDAssignedPacket(clientIDPacket,info); });
	Log logger = Log("ClientLink");
	struct IndexByState
	{
	};
	struct IndexByTarget
	{
	};
	struct IndexByHSteamNetConnection
	{
	};
	boost::multi_index_container<
		Connection,
		boost::multi_index::indexed_by<
			// non-unique by connection state
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByState>,
				boost::multi_index::member<Connection, ConnectionState,
										   &Connection::state>>,
			// non-unique, the reason its non unique is because GameClient on
			// connection dont have an ID
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByTarget>,
				boost::multi_index::member<Connection, NetworkIdentity,
										   &Connection::target>>,
			// Unique By HConnection
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByHSteamNetConnection>,
				boost::multi_index::member<Connection, HSteamNetConnection,
										   &Connection::SteamConnection>>>>
		Connections;
};

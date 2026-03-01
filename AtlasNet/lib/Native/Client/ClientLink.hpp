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

	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);

	PacketManager& GetPacketManager() { return packet_manager; }

	constexpr const NetworkIdentity& GetManagingProxy() const {return ManagingProxy;}
   private:
	void OnConnected(SteamNetConnectionStatusChangedCallback_t* pInfo);
	void Update();

	void InitGNS();

	void ReceiveMessages();

	void OnClientIDAssignedPacket(const ClientIDAssignPacket& clientIDPacket,
								  const PacketManager::PacketInfo&);

	std::jthread TickThread;
	HSteamNetPollGroup poll_group;
	PacketManager packet_manager;
	PacketManager::Subscription ClientIDAssignSub;
	Log logger = Log("ClientLink");

	NetworkIdentity ManagingProxy;

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
				boost::multi_index::member<Connection, ConnectionState, &Connection::state>>,
			// non-unique, the reason its non unique is because GameClient on
			// connection dont have an ID
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByTarget>,
				boost::multi_index::member<Connection, NetworkIdentity, &Connection::target>>,
			// Unique By HConnection
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByHSteamNetConnection>,
				boost::multi_index::member<Connection, HSteamNetConnection,
										   &Connection::SteamConnection>>>>
		Connections;
};

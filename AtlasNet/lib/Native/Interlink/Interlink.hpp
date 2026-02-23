#pragma once
#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Docker/DockerIO.hpp"
#include "GameNetworkingSockets.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
#include "InterlinkEnums.hpp"
#include "Network/Connection.hpp"
#include "Network/ConnectionTelemetry.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
#include "Network/Packet/PacketManager.hpp"

struct InterlinkProperties
{
	NetworkIdentity ThisID;
};
inline NetworkIdentityType GetTargetType(const Connection &c)
{
	return c.target.Type;
}
using InterlinkContainereID = DockerContainerID;
class Interlink : public Singleton<Interlink>
{
	struct IndexByState
	{
	};
	struct IndexByTarget
	{
	};
	struct IndexByTargetType
	{
	};
	struct IndexByHSteamNetConnection
	{
	};
	/**
	 * @brief Container that stores values by multiple indicies
	 * 1. By State, Non unique
	 * 2. By TargetType, Non Unique
	 *
	 */
	boost::multi_index_container<
		Connection,
		boost::multi_index::indexed_by<
			// non-unique by connection state
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByState>,
				boost::multi_index::member<Connection, ConnectionState, &Connection::state>>,
			// non-unique by target interlinkType
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByTargetType>,
				boost::multi_index::global_fun<const Connection &, NetworkIdentityType,
											   GetTargetType>>,
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

	std::unordered_map<NetworkIdentity,
					   std::vector<std::pair<std::shared_ptr<IPacket>, NetworkMessageSendFlag>>>
		QueuedPacketsOnConnect;
	Log logger = Log("Interlink");
	ISteamNetworkingSockets *networkInterface;
	std::optional<HSteamListenSocket> ListeningSocket;
	std::optional<HSteamNetPollGroup> PollGroup;
	PacketManager packet_manager;
	bool b_InDockerNetwork = true;
	std::atomic_bool IsInit = false;

   public:
	bool EstablishConnectionAtIP(const NetworkIdentity &who, const IPAddress &ip);
	void CloseConnectionTo(const NetworkIdentity &id, int reason = 0, const char *debug = nullptr);
	void CloseAllConnections(int reason = 0);
	bool EstablishConnectionTo(const NetworkIdentity &who);

   private:
	void GenerateNewConnections();
	void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg);

	template <typename... Args>
	void Debug(std::string_view fmt, Args &&...args) const
	{
		std::string msg = "Interlink> " + std::string(fmt);
		logger.Debug(msg, std::forward<Args>(args)...);
	}
	static inline std::string SecretDatabaseKey = "SECRET";
	using SteamCBInfo = SteamNetConnectionStatusChangedCallback_t *;
	void CallbackOnConnecting(SteamCBInfo info);
	void CallbackOnClosedByPear(SteamCBInfo info);
	void CallbackOnProblemDetectedLocally(SteamCBInfo info);
	void CallbackOnConnected(SteamCBInfo info);
	void OpenListenSocket(PortType port);
	void ReceiveMessages();

	// void DebugPrint();
	void OnClientConnected(const Connection &c);

	void Tick();
	std::jthread TickThread;

   public:
	void Init();
	void Shutdown();

	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void GetConnectionTelemetry(std::vector<ConnectionTelemetry> &out);
	// void SendMessageRaw(const InterLinkIdentifier &who, std::span<const
	// std::byte> data, InterlinkMessageSendFlag sendFlag =
	// InterlinkMessageSendFlag::eReliableBatched);
	template <typename T>
	void SendMessage(const NetworkIdentity &who, const T &packet, NetworkMessageSendFlag sendFlag)
	{
		std::shared_ptr<IPacket> packet_ptr = std::make_shared<T>(packet);
		SendMessage(who, packet_ptr, sendFlag);
	}
	void SendMessage(const NetworkIdentity &who, const std::shared_ptr<IPacket> &packet,
					 NetworkMessageSendFlag sendFlag);
	PacketManager &GetPacketManager()
	{
		ASSERT(IsInit, "Interlink was not initialized");
		return packet_manager;
	}
};

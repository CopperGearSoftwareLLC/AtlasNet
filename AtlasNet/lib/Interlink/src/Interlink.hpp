#pragma once
#include <type_traits>

#include "Connection.hpp"
#include "DockerIO.hpp"
#include "GameNetworkingSockets.hpp"
#include "InterlinkEnums.hpp"
#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Packet/Packet.hpp"
#include "Packet/PacketManager.hpp"
#include "pch.hpp"
struct InterlinkCallbacks
{
	std::function<bool(const Connection &)> acceptConnectionCallback;
	std::function<void(const InterLinkIdentifier &)> OnConnectedCallback;
	std::function<void(const InterLinkIdentifier &)> OnDisconnectedCallback;
};
struct InterlinkProperties
{
	InterLinkIdentifier ThisID;
	std::shared_ptr<Log> logger;
	InterlinkCallbacks callbacks;
};
inline InterlinkType GetTargetType(const Connection &c)
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
			// non-unique by state
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByState>,
				boost::multi_index::member<Connection, ConnectionState, &Connection::state>>,
			// non-unique by target interlinkType
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByTargetType>,
				boost::multi_index::global_fun<const Connection &, InterlinkType, GetTargetType>>,
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<IndexByTarget>,
				boost::multi_index::member<Connection, InterLinkIdentifier, &Connection::target>>,
			// Unique By HConnection
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByHSteamNetConnection>,
				boost::multi_index::member<Connection, HSteamNetConnection,
										   &Connection::SteamConnection>>>>
		Connections;

	std::shared_ptr<Log> logger;
	ISteamNetworkingSockets *networkInterface;
	InterLinkIdentifier MyIdentity;
	std::optional<HSteamListenSocket> ListeningSocket;
	std::optional<HSteamNetPollGroup> PollGroup;
	PacketManager packet_manager;
	InterlinkCallbacks callbacks;
	PortType ListenPort;
	bool b_InDockerNetwork = true;
	const static inline std::unordered_map<InterlinkType, uint32> Type2ListenPort = {
		{InterlinkType::eWatchDog, _PORT_WATCHDOG},
		{InterlinkType::eShard, _PORT_SHARD},
		{InterlinkType::eGameServer, _PORT_GAMESERVER},
		{InterlinkType::eProxy, _PORT_PROXY},
		{InterlinkType::eGameClient, 25569}	 // temp client port
	};

   public:
	bool EstablishConnectionAtIP(const InterLinkIdentifier &who, const IPAddress &ip);
	void CloseConnectionTo(const InterLinkIdentifier &id, int reason = 0,
						   const char *debug = nullptr);
	bool EstablishConnectionTo(const InterLinkIdentifier &who);

   private:
	void DispatchDisconnected(const InterLinkIdentifier &id);
	void GenerateNewConnections();
	void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg);

	template <typename... Args>
	void Debug(std::string_view fmt, Args &&...args) const
	{
		std::string msg = "Interlink> " + std::string(fmt);
		logger->Debug(msg, std::forward<Args>(args)...);
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

   public:
	void Init(const InterlinkProperties &properties);
	void Shutdown();

	void Tick();
	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);

	// void SendMessageRaw(const InterLinkIdentifier &who, std::span<const std::byte> data,
	// InterlinkMessageSendFlag sendFlag = InterlinkMessageSendFlag::eReliableBatched);
	template <typename T, std::enable_if_t<std::is_base_of_v<IPacket, T>>>
	void SendMessage(const InterLinkIdentifier &who, const T &packet,
					 InterlinkMessageSendFlag sendFlag);
	void SendMessage(const InterLinkIdentifier &who, const std::shared_ptr<IPacket> &packet,
					 InterlinkMessageSendFlag sendFlag);
	PacketManager &GetPacketManager() { return packet_manager; }
};

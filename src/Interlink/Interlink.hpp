#pragma once
#include "Connection.hpp"
#include "Debug/Log.hpp"
#include "Globals.hpp"
#include "Singleton.hpp"
#include "pch.hpp"
#include "InterlinkMessage.hpp"
#include "Docker/DockerIO.hpp"

struct InterlinkCallbacks
{
	std::function<bool(const Connection &)> acceptConnectionCallback;
	std::function<void(const InterLinkIdentifier &)> OnConnectedCallback;
	std::function<void(const Connection &, std::span<const std::byte>)> OnMessageArrival;
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
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<IndexByHSteamNetConnection>,
				boost::multi_index::member<Connection, HSteamNetConnection,
										   &Connection::SteamConnection>>>>
		Connections;

	std::shared_ptr<Log> logger;
	ISteamNetworkingSockets *networkInterface;
	InterLinkIdentifier MyIdentity;
	std::optional<HSteamListenSocket> ListeningSocket;
	std::optional<HSteamNetPollGroup> PollGroup;

	InterlinkCallbacks callbacks;
	PortType ListenPort;

	const static inline std::unordered_map<InterlinkType, uint32> Type2ListenPort = {{InterlinkType::eGod, _PORT_GOD},
																					 {InterlinkType::ePartition, _PORT_PARTITION},
																					 {InterlinkType::eGameServer, _PORT_GAMESERVER}};

private:
	void GenerateNewConnections();

	void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg);

	template <typename... Args>
	void Debug(std::string_view fmt, Args &&...args) const
	{
		std::string msg = "Interlink> " + std::string(fmt);
		logger->Debug(msg, std::forward<Args>(args)...);
	}

	using SteamCBInfo = SteamNetConnectionStatusChangedCallback_t *;
	void CallbackOnConnecting(SteamCBInfo info);
	void CallbackOnConnected(SteamCBInfo info);
	void OpenListenSocket(PortType port);
	void ReceiveMessages();

	bool EstablishConnectionTo(const InterLinkIdentifier &who);
	
	public:
	void Init(const InterlinkProperties &properties);
	void Shutdown();
	
	void Tick();
	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);

	void SendMessageRaw(const InterLinkIdentifier& who, std::span<const std::byte> data,InterlinkMessageSendFlag sendFlag = InterlinkMessageSendFlag::eReliableBatched);
	void SendMessage(const InterlinkMessage &message);
};

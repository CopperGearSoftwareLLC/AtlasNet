#pragma once
#include "Connection.hpp"
#include "Debug/Log.hpp"
#include "Globals.hpp"
#include "Singleton.hpp"
#include "pch.hpp"
#include "InterlinkMessage.hpp"
#include "Docker/DockerIO.hpp"
const static inline int32 CJ_LOCALHOST_PARTITION_PORT = 25565;
using AcceptConnectionFunc = std::function<bool(const Connection &)>;
struct InterlinkProperties
{
	InterLinkIdentifier ThisID;
	std::shared_ptr<Log> logger;

	bool bOpenListenSocket = false;
	PortType ListenSocketPort = -1;
	AcceptConnectionFunc acceptConnectionFunc;
};
using InterlinkContainereID = DockerContainerID;
class Interlink : public Singleton<Interlink>
{
	struct IndexByState
	{
	};
	struct IndexByTarget
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
			// non-unique by target
			boost::multi_index::ordered_non_unique<
				boost::multi_index::tag<IndexByTarget>,
				boost::multi_index::member<Connection, InterlinkType, &Connection::TargetType>>,
			// Unique By HConnection
			boost::multi_index::ordered_unique<
				boost::multi_index::tag<IndexByHSteamNetConnection>,
				boost::multi_index::member<Connection, HSteamNetConnection,
										   &Connection::Connection>>

			>>
		Connections;

	using ConnectionRef = decltype(Connections)::iterator;
	// std::multiset<Connection, ConnectionStateHash> ConnectionsByState;
	// std::unordered_map<ConnectionState, std::multiset<Connection,ConnectionCompareTargetType>>
	// Connections;
	std::shared_ptr<Log> logger;
	ISteamNetworkingSockets *networkInterface;
	InterLinkIdentifier MyIdentity;
	std::optional<HSteamListenSocket> ListeningSocket;
	std::optional<HSteamNetPollGroup> PollGroup;

	AcceptConnectionFunc _acceptConnectionFunc;

  private:
	void GenerateNewConnections();

	void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg);

	template <typename... Args> void Print(std::string_view fmt, Args &&...args) const
	{
		std::string msg = "Interlink> " + std::string(fmt);
		logger->Print(msg, std::forward<Args>(args)...);
	}

	using SteamCBInfo = SteamNetConnectionStatusChangedCallback_t*;
	void CallbackOnConnecting(SteamCBInfo info);
	void CallbackOnConnected(SteamCBInfo info);
  public:
	void Init(const InterlinkProperties &properties);
	void Shutdown();
	ConnectionRef ConnectTo(const ConnectionProperties &ConnectProps);
	ConnectionRef ConnectToLocalParition();

	void Tick();
	void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo);
	void SendMessage(const InterlinkMessage& message);
};

#include "Interlink/Interlink.hpp"

#include "Interlink.hpp"

void Interlink::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState)
	{
		// Somebody is trying to connect
	case k_ESteamNetworkingConnectionState_Connecting:
		logger->Print("k_ESteamNetworkingConnectionState_Connecting");
		CallbackOnConnecting(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_Connected:
		logger->Print("k_ESteamNetworkingConnectionState_Connected");
		CallbackOnConnected(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
		logger->Print("k_ESteamNetworkingConnectionState_ClosedByPeer");
		break;
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		logger->Print("k_ESteamNetworkingConnectionState_ProblemDetectedLocally");
		break;
	case k_ESteamNetworkingConnectionState_FinWait:
		logger->Print("k_ESteamNetworkingConnectionState_FinWait");
		break;

	case k_ESteamNetworkingConnectionState_Dead:
		logger->Print("k_ESteamNetworkingConnectionState_Dead");
		break;

	default:
		logger->Print(std::format("Unknown {}", (int64)pInfo->m_info.m_eState));
	};
}
static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
	Interlink::Get().OnSteamNetConnectionStatusChanged(info);
}

void Interlink::GenerateNewConnections()
{
	auto &IndiciesByState = Connections.get<IndexByState>();
	auto PreConnectingConnections = IndiciesByState.equal_range(ConnectionState::ePreConnecting);
	// auto& ByState = Connections.equal_range(ConnectionState::ePreConnecting);
	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			   (void *)SteamNetConnectionStatusChanged);
	for (auto it = PreConnectingConnections.first; it != PreConnectingConnections.second; ++it)
	{
		const Connection &connection = *it;
		logger->PrintFormatted("Connecting to {}", connection.address.ToString());

		HSteamNetConnection conn =
			networkInterface->ConnectByIPAddress(connection.address.ToSteamIPAddr(), 1, &opt);
		if (conn == k_HSteamNetConnection_Invalid)
		{
			logger->PrintFormatted("Failed to Generate New Connection {}", connection.address.ToString());
		}
		else
		{
			IndiciesByState.modify(it, [conn = conn](Connection &c)
								   { c.Connection = conn; });
		}
	}
}

void Interlink::OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
{
	logger->PrintFormatted(std::format("[GNS Debug] {}\n", pszMsg));
}

void Interlink::CallbackOnConnecting(SteamCBInfo info)
{

	auto &IndiciesBySteamConnection = Connections.get<IndexByHSteamNetConnection>();
	auto ExistingConnection = IndiciesBySteamConnection.find(info->m_hConn);

	// if connection is already registered then I initiated
	if (ExistingConnection != IndiciesBySteamConnection.end())
	{
		IndiciesBySteamConnection.modify(
			ExistingConnection, [](Connection &c)
			{ c.SetNewState(ConnectionState::eConnecting); });
		return;
	}
	else // if it is not then its not mine, this is a new incoming connection
	{

		// IdentityPacket identity = IdentityPacket::FromString(info->m_info.m_nUserData);
		Connection newCon;
		newCon.Connection = info->m_hConn;
		newCon.SetNewState(ConnectionState::eConnecting);
		newCon.address = IPAddress(info->m_info.m_addrRemote);
		newCon.TargetType = InterlinkType::eUnknown;
		// request user for permission to connect
		if (EResult result = networkInterface->AcceptConnection(newCon.Connection);
			result != k_EResultOK)
		{

			logger->PrintFormatted("Error accepting connection: {}", uint64(result));
			networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
			throw std::runtime_error(
				"This exception is because I have not implemented what to do when it fails "
				"so i want to know when it does and not have undefined behaviour");
		}
		else
		{
			Connections.insert(newCon);
		}
	}
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
	printf("OnConnected!\n");
}

void Interlink::Init(const InterlinkProperties &Properties)
{

	MyIdentity = Properties.ThisID;
	ASSERT(Properties.logger, "Invalid Logger");
	logger = Properties.logger;

	logger->Print("Interlink init");
	ASSERT(MyIdentity.Type != InterlinkType::eInvalid, "Invalid Interlink Type");

	ASSERT(Properties.acceptConnectionFunc,
		   "You must provide a function for accepting connections");
	_acceptConnectionFunc = Properties.acceptConnectionFunc;
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{

		logger->Print(std::string("GameNetworkingSockets_Init failed: ") + errMsg);
		return;
	}
	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Msg,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
		{
			Interlink::Get().OnDebugOutput(eType, pszMsg);
		});
	networkInterface = SteamNetworkingSockets();

	if (Properties.bOpenListenSocket)
	{
		ASSERT(Properties.ListenSocketPort != -1, "Invalid Port");
		SteamNetworkingConfigValue_t opt;
		opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				   (void *)SteamNetConnectionStatusChanged);
		ListeningSocket = networkInterface->CreateListenSocketIP(
			IPAddress::MakeLocalHost(Properties.ListenSocketPort).ToSteamIPAddr(), 1, &opt);
		if (ListeningSocket.value() == k_HSteamListenSocket_Invalid)
		{
			throw std::runtime_error(
				std::format("Failed to listen on port {}", Properties.ListenSocketPort));
		}
	}
}

void Interlink::Shutdown()
{
	logger->Print("Interlink Shutdown");
}

Interlink::ConnectionRef Interlink::ConnectTo(const ConnectionProperties &ConnectProps)
{
	Connection connection;
	connection.oldState = ConnectionState::eInvalid;
	connection.state = ConnectionState::ePreConnecting;
	connection.address = ConnectProps.address;
	connection.TargetType = InterlinkType::eUnknown;

	// connections.push_back(connection);
	// AddToConnections(connection);
	auto ret = Connections.insert(connection);
	return ret.first;
}
Interlink::ConnectionRef Interlink::ConnectToLocalParition()
{
	return ConnectTo(
		ConnectionProperties{.address = IPAddress::MakeLocalHost(CJ_LOCALHOST_PARTITION_PORT)});
}
void Interlink::Tick()
{
	GenerateNewConnections();
	networkInterface->RunCallbacks(); // process events
									  // logger->Print("tick");
}

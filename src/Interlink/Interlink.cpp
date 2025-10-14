#include "Interlink/Interlink.hpp"

#include "Interlink.hpp"
#include "Database/ServerRegistry.hpp"
void Interlink::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState)
	{
		// Somebody is trying to connect
	case k_ESteamNetworkingConnectionState_Connecting:

		CallbackOnConnecting(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_Connected:
		CallbackOnConnected(pInfo);
		break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
		logger->Debug("k_ESteamNetworkingConnectionState_ClosedByPeer");
		break;
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
		logger->Debug("k_ESteamNetworkingConnectionState_ProblemDetectedLocally");
		break;
	case k_ESteamNetworkingConnectionState_FinWait:
		logger->Debug("k_ESteamNetworkingConnectionState_FinWait");
		break;

	case k_ESteamNetworkingConnectionState_Dead:
		logger->Debug("k_ESteamNetworkingConnectionState_Dead");
		break;

	default:
		logger->ErrorFormatted(std::format("Unknown {}", (int64)pInfo->m_info.m_eState));
	};
}
void Interlink::SendMessage(const InterlinkMessage &message)
{
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
	SteamNetworkingConfigValue_t opt[1];
	opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				  (void *)SteamNetConnectionStatusChanged);
	for (auto it = PreConnectingConnections.first; it != PreConnectingConnections.second; ++it)
	{
		const Connection &connection = *it;
		logger->DebugFormatted("Connecting to {} at {}", connection.target.ToString(), connection.address.ToString());

		HSteamNetConnection conn =
			networkInterface->ConnectByIPAddress(connection.address.ToSteamIPAddr(), 1, opt);
		if (conn == k_HSteamNetConnection_Invalid)
		{
			logger->ErrorFormatted("Failed to Generate New Connection {}", connection.address.ToString());
		}
		else
		{
			IndiciesByState.modify(it, [conn = conn](Connection &c)
								   { c.SteamConnection = conn; });
		}
	}
}

void Interlink::OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
{
	logger->DebugFormatted(std::format("[GNS Debug] {}\n", pszMsg));
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
		newCon.SteamConnection = info->m_hConn;
		newCon.SetNewState(ConnectionState::eConnecting);
		newCon.address = IPAddress(info->m_info.m_addrRemote);
		int identityByteStreamSize;
		const std::byte *identityByteStream = (std::byte *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);
		ASSERT(identityByteStream, "No Bytestream identity");
		/*
		std::string byteStream2String;
		for (int i = 0; i < identityByteStreamSize; i++)
			byteStream2String.push_back((char)identityByteStream[i]);
		logger->DebugFormatted("Incoming connection ByteStream identity {} size: {}", byteStream2String, identityByteStreamSize);
		ASSERT(identityByteStreamSize <= 32, "Invalid Identity Byte Stream");
*/
		if (info->m_info.m_identityRemote.m_eType != k_ESteamNetworkingIdentityType_GenericBytes)
		{
			logger->WarningFormatted("UNknown incoming connection {}. NetworkIdentity not a byte stream . Type: {}", newCon.address.ToString(), (int32)info->m_info.m_identityRemote.m_eType);
			ASSERT(false, "Identity has to be byte stream so we can identity by InterlinkIdentifier and server manifest");
		}
		const auto ID = InterLinkIdentifier::FromEncodedByteStream(identityByteStream, identityByteStreamSize);
		if (!ID.has_value())
		{
			logger->WarningFormatted("unknown byte stream  in networkIdentity");
			ASSERT(false, "Received connecting from something unknown");
		}

		if (!ServerRegistry::Get().ExistsInRegistry(ID.value()))
		{
			logger->ErrorFormatted("Incoming connection from identity '{}' could not be verified since it was not in the registry", ID->ToString());

			ASSERT(false, "Received connecting from something not in the server registry");
		}
		logger->DebugFormatted("Incoming Connection from: {} at {}", ID->ToString(), newCon.address.ToString());
		newCon.target = ID.value();
		// request user for permission to connect
		if (EResult result = networkInterface->AcceptConnection(newCon.SteamConnection);
			result != k_EResultOK)
		{

			logger->ErrorFormatted("Error accepting connection: {}", uint64(result));
			networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
			throw std::runtime_error(
				"This exception is because I have not implemented what to do when it fails "
				"so i want to know when it does and not have undefined behaviour");
		}
		else
		{ // logger->Debug("Establishing connection to ")
			Connections.insert(newCon);
		}
	}
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
	info->m_hConn;
	auto &indiciesBySteamConn = Connections.get<IndexByHSteamNetConnection>();
	if (auto v = indiciesBySteamConn.find(info->m_hConn); v != indiciesBySteamConn.end())
	{
		std::vector<std::pair<InterlinkMessageSendFlag, std::vector<std::byte>>> MessagesToSend;
		indiciesBySteamConn.modify(v, [&MessagesToSend = MessagesToSend](Connection &c)
								   { c.SetNewState(ConnectionState::eConnected);
								MessagesToSend =std::move(c.MessagesToSendOnConnect); });
		bool result = networkInterface->SetConnectionPollGroup(v->SteamConnection, PollGroup.value());
		if (!result)
		{
			logger->ErrorFormatted("Failed to assign connection from {} to pollgroup", v->target.ToString());
		}
		logger->DebugFormatted(" - {} Connected", v->target.ToString());
		if (!MessagesToSend.empty())
		{
			for (const auto &msg : MessagesToSend)
			{
				SendMessageRaw(v->target, msg.second, msg.first);
			}
			logger->DebugFormatted("Sent {} messages on Connect to {}", MessagesToSend.size(), v->target.ToString());
		}
	}
	else
	{
		ASSERT(false, "Connected? to non existent connection?");
	}
}

void Interlink::OpenListenSocket(PortType port)
{
	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			   (void *)SteamNetConnectionStatusChanged);
	SteamNetworkingIPAddr addr;
	addr.ParseString("0.0.0.0");
	addr.m_port = port;
	ListeningSocket = networkInterface->CreateListenSocketIP(
		addr, 1, &opt);
	if (ListeningSocket.value() == k_HSteamListenSocket_Invalid)
	{
		logger->ErrorFormatted("Failed to listen on port {}", port);

		throw std::runtime_error(
			std::format("Failed to listen on port {}", port));
	}
	logger->DebugFormatted("Opened listen socket on port {}", port);
}

void Interlink::ReceiveMessages()
{
	ISteamNetworkingMessage *pIncomingMessages[32];
	int numMsgs = networkInterface->ReceiveMessagesOnPollGroup(PollGroup.value(), pIncomingMessages, 32);

	for (int i = 0; i < numMsgs; ++i)
	{
		ISteamNetworkingMessage *msg = pIncomingMessages[i];

		// Access message data
		const void *data = msg->m_pData;
		size_t size = msg->m_cbSize;
		const Connection &sender = *Connections.get<IndexByHSteamNetConnection>().find(msg->m_conn);

		callbacks.OnMessageArrival(sender,std::span<const std::byte>((const std::byte*)data,size));
		// Example: interpret as string
		std::string text(reinterpret_cast<const char *>(data), size);
		logger->DebugFormatted("Message from ({}) \"{}\"", sender.target.ToString(), text);
		// Free message when done
		msg->Release();
	}
}

void Interlink::Init(const InterlinkProperties &Properties)
{

	MyIdentity = Properties.ThisID;
	ASSERT(Properties.logger, "Invalid Logger");
	logger = Properties.logger;

	logger->Debug("Interlink init");
	ASSERT(MyIdentity.Type != InterlinkType::eInvalid, "Invalid Interlink Type");

	ASSERT(Properties.callbacks.acceptConnectionCallback,
		   "You must provide a function for accepting connections");
	callbacks = Properties.callbacks;
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{

		logger->Error(std::string("GameNetworkingSockets_Init failed: ") + errMsg);
		return;
	}
	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Warning,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
		{
			Interlink::Get().OnDebugOutput(eType, pszMsg);
		});
	networkInterface = SteamNetworkingSockets();
	const auto IdentityByteStream = MyIdentity.ToEncodedByteStream();
	logger->DebugFormatted("Settings Networking Identity");
	SteamNetworkingIdentity identity;
	const bool SetIdentity = identity.SetGenericBytes(IdentityByteStream.data(), IdentityByteStream.size());
	//=identity.SetGenericString(NetworkingIdentityString.c_str());
	ASSERT(SetIdentity, "Failed Identity set");
	networkInterface->ResetIdentity(&identity);
	// ASSERT(identity.GetGenericBytes() == NetworkingIdentityString, "Invalid Identity set");
	/*

	SteamNetworkingIdentity Getidentity;
	if (bool GetIdentity = networkInterface->GetIdentity(&Getidentity); !GetIdentity || (Getidentity.GetGenericString() != MyIdentity.ToString()))
	{
		logger->ErrorFormatted("Failure to set networking identity: {}", Getidentity.GetGenericString());
	}*/
	PollGroup = networkInterface->CreatePollGroup();
	IPAddress ipAddress;
	ListenPort = Type2ListenPort.at(MyIdentity.Type);
	ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":" + std::to_string(ListenPort));
	ServerRegistry::Get().RegisterSelf(MyIdentity, ipAddress);
	logger->DebugFormatted("Registered in ServerRegistry as {}", MyIdentity.ToString());
	OpenListenSocket(ListenPort);
	switch (MyIdentity.Type)
	{
	case InterlinkType::eGameServer:
	{

		InterLinkIdentifier PartitionID = MyIdentity;
		PartitionID.Type = InterlinkType::ePartition;
		EstablishConnectionTo(PartitionID);
	}
	break;
	case InterlinkType::ePartition:

		EstablishConnectionTo(InterLinkIdentifier::MakeIDGod());

		break;
	case InterlinkType::eGod:

		break;
	default:
		break;
	}
}

void Interlink::Shutdown()
{
	logger->Debug("Interlink Shutdown");
}

bool Interlink::EstablishConnectionTo(const InterLinkIdentifier &id)
{
	if (Connections.get<IndexByTarget>().contains(id))
	{
		const Connection &Existingconnection = *Connections.get<IndexByTarget>().find(id);
		if (Existingconnection.state == ConnectionState::eConnected)
		{
			logger->WarningFormatted("Connection to {} already established", id.ToString());
		}
		else if (Existingconnection.state == ConnectionState::eConnecting || Existingconnection.state == ConnectionState::ePreConnecting)
			return true;
		else
		{
			logger->ErrorFormatted("Undefined connection state in EstablishConnectionTo. State: {}", boost::describe::enum_to_string(Existingconnection.state, "unknownstate?"));
		}

		return true;
	}
	auto IP = ServerRegistry::Get().GetIPOfID(id);

	for (int i = 0; i < 5; i++)
	{
		if (!IP.has_value())
		{
			logger->ErrorFormatted("IP not found for {} in Server Registry. Trying again in 1 second", id.ToString());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			IP = ServerRegistry::Get().GetIPOfID(id);
		}
	}
	if (!IP.has_value())
	{
		logger->ErrorFormatted("Failed to establish connection after 5 tries. IP not found in Server Registry {}", id.ToString());
		ASSERT(false, "Failed to establish connection, could not find IP");
		return false;
	}
	Connection connec;
	SteamNetworkingIPAddr addr = IP->ToSteamIPAddr();
	addr.m_port = _PORT_GOD;
	// std::string s = std::string(":")+ std::to_string(_PORT_GOD);
	// addr.ParseString(s.c_str());
	IPAddress ip2(addr);

	logger->DebugFormatted("Establishing connection to {}", id.ToString());
	connec.address = IP.value();
	connec.target = id;
	connec.SetNewState(ConnectionState::ePreConnecting);
	// connections.push_back(connection);
	// AddToConnections(connection);
	Connections.insert(connec);
	return true;
}

void Interlink::SendMessageRaw(const InterLinkIdentifier &who, std::span<const std::byte> data, InterlinkMessageSendFlag sendFlag)
{
	if (!Connections.get<IndexByTarget>().contains(who) || Connections.get<IndexByTarget>().find(who)->state != ConnectionState::eConnected)
	{
		EstablishConnectionTo(who);

		auto it = Connections.get<IndexByTarget>().find(who);
		Connections.get<IndexByTarget>().modify(it, [data = data, sendFlag](Connection &c)
												{
													std::vector<std::byte> newdata;
													newdata.insert(newdata.end(), data.begin(), data.end());
													c.MessagesToSendOnConnect.push_back(std::make_pair(sendFlag,std::move(newdata))); });
	}
	else
	{
		const Connection &conn = *Connections.get<IndexByTarget>().find(who);
		const auto SendResult = networkInterface->SendMessageToConnection(conn.SteamConnection, data.data(), data.size_bytes(), (int)sendFlag, nullptr);

		if (SendResult != k_EResultOK)
		{

			logger->ErrorFormatted("Unable to send message. SteamNetworkingSockets returned result code {}", (int)SendResult);
			ASSERT(false, "Unable to send message");
		}
		else
		{

			logger->DebugFormatted("Message of size {} bytes sent to {}", data.size_bytes(), who.ToString());
		}
	}
}

void Interlink::Tick()
{
	GenerateNewConnections();
	ReceiveMessages();
	networkInterface->RunCallbacks(); // process events
									  // logger->Debug("tick");
}

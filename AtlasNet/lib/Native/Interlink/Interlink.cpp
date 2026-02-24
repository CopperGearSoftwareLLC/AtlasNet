#include "Interlink.hpp"

#include <steam/steamtypes.h>

#include <atomic>
#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <stop_token>
#include <thread>

// #include "Database/ProxyRegistry.hpp"
#include "Client/Client.hpp"
#include "Client/ClientManifest.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"
#include "Events/EventSystem.hpp"
#include "Events/Events/Client/ClientEvents.hpp"
#include "GameNetworkingSockets.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Handshake/HandshakeService.hpp"
#include "InterlinkEnums.hpp"
#include "Network/Connection.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Client/ClientIDAssignPacket.hpp"
#include "Network/Packet/Packet.hpp"
#include "Network/Packet/PacketManager.hpp"
#include "steam/steamclientpublic.h"

// ===== Safe, single-process guard for GNS init ===============================
static bool EnsureGNSInitialized()
{
	static std::atomic<bool> initialized{false};
	if (initialized.load(std::memory_order_acquire))
		return true;

	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{
		std::cerr << (std::string("GameNetworkingSockets_Init failed: ") + errMsg) << std::endl;
		return false;
	}
	initialized.store(true, std::memory_order_release);
	return true;
}

// ===== Static forwarder for GNS connection status callback ===================
static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
	Interlink::Get().OnSteamNetConnectionStatusChanged(info);
}

// ===== Interlink implementation =============================================
void Interlink::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState)
	{
		case k_ESteamNetworkingConnectionState_Connecting:
			CallbackOnConnecting(pInfo);
			break;
		case k_ESteamNetworkingConnectionState_Connected:
			CallbackOnConnected(pInfo);
			break;
		case k_ESteamNetworkingConnectionState_ClosedByPeer:
			CallbackOnClosedByPear(pInfo);
			logger.Debug("k_ESteamNetworkingConnectionState_ClosedByPeer");
			break;
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			CallbackOnProblemDetectedLocally(pInfo);
			logger.Debug("k_ESteamNetworkingConnectionState_ProblemDetectedLocally");
			logger.ErrorFormatted("[GNS] ProblemDetectedLocally. desc='{}' reason={} debug='{}'",
								  pInfo->m_info.m_szConnectionDescription,
								  (int)pInfo->m_info.m_eEndReason, pInfo->m_info.m_szEndDebug);
			break;
		case k_ESteamNetworkingConnectionState_FinWait:
			logger.Debug("k_ESteamNetworkingConnectionState_FinWait");
			break;
		case k_ESteamNetworkingConnectionState_Dead:
			logger.Debug("k_ESteamNetworkingConnectionState_Dead");
			break;
		case k_ESteamNetworkingConnectionState_None:
			break;
		default:
			logger.ErrorFormatted(std::format("Unknown {}", (int64)pInfo->m_info.m_eState));
			break;
	}
}

void Interlink::SendMessage(const NetworkIdentity &who, const std::shared_ptr<IPacket> &packet,
							NetworkMessageSendFlag sendFlag)
{
	ASSERT(IsInit, "Interlink was not initialized");
	ASSERT(packet->Validate(), "Packet did not valite");
	if (!packet->Validate())
	{
		logger.ErrorFormatted(
			"Interlink::SendMessage: Packet of type {} did not validate "
			"successfully",
			packet->GetPacketName());
		return;
	}

	auto QueueMessageOnConnect = [&]()
	{ QueuedPacketsOnConnect[who].push_back(std::make_pair(packet, sendFlag)); };

	if (!Connections.get<IndexByTarget>().contains(who))
	{
		logger.DebugFormatted("Connection to \"{}\" was not established, connecting...",
							  who.ToString());
		EstablishConnectionTo(who);
		QueueMessageOnConnect();
	}
	else if (auto find = Connections.get<IndexByTarget>().find(who);
			 find->state != ConnectionState::eConnected)
	{
		if (find->state == ConnectionState::ePreConnecting ||
			find->state == ConnectionState::eConnecting)
		{
			logger.DebugFormatted(
				"Connection to {} is {} , queuing message...", who.ToString(),
				boost::describe::enum_to_string(Connections.get<IndexByTarget>().find(who)->state,
												"unknown"));
			QueueMessageOnConnect();
		}
		else
		{
			logger.DebugFormatted(
				"Connection to {} state is {} , connecting...", who.ToString(),
				boost::describe::enum_to_string(Connections.get<IndexByTarget>().find(who)->state,
												"unknown"));
			EstablishConnectionTo(who);
			QueueMessageOnConnect();
		}
	}
	else
	{
		const Connection &conn = *Connections.get<IndexByTarget>().find(who);

		ByteWriter bw;
		packet->Serialize(bw);
		const auto data_span = bw.bytes();
		const auto SendResult = networkInterface->SendMessageToConnection(
			conn.SteamConnection, data_span.data(), data_span.size_bytes(), (int)sendFlag, nullptr);

		if (SendResult != k_EResultOK)
		{
			logger.ErrorFormatted(
				"Unable to send message. SteamNetworkingSockets returned "
				"result code {}",
				(int)SendResult);
		}
		else
		{
			//logger.DebugFormatted("{} Packet sent to {}", packet->GetPacketName(), who.ToString());
		}
	}
}

void Interlink::GenerateNewConnections()
{
	auto &IndiciesByState = Connections.get<IndexByState>();
	auto PreConnectingConnections = IndiciesByState.equal_range(ConnectionState::ePreConnecting);

	SteamNetworkingConfigValue_t opt[1];
	opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				  (void *)SteamNetConnectionStatusChanged);

	for (auto it = PreConnectingConnections.first; it != PreConnectingConnections.second; ++it)
	{
		const Connection &connection = *it;
		logger.DebugFormatted("Connecting to {} at {}", connection.target.ToString(),
							  connection.address.ToString());

		HSteamNetConnection conn =
			networkInterface->ConnectByIPAddress(connection.address.ToSteamIPAddr(), 1, opt);
		if (conn == k_HSteamNetConnection_Invalid)
		{
			logger.ErrorFormatted("Failed to Generate New Connection {}",
								  connection.address.ToString());
		}
		else
		{
			IndiciesByState.modify(it, [conn = conn](Connection &c) { c.SteamConnection = conn; });
		}
	}
}

void Interlink::OnDebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
{
	logger.DebugFormatted(std::format("[GNS Debug] {}\n", pszMsg));
}

void Interlink::CallbackOnConnecting(SteamCBInfo info)
{
	logger.DebugFormatted("On Connecting");

	auto &IndiciesBySteamConnection = Connections.get<IndexByHSteamNetConnection>();
	auto ExistingConnection = IndiciesBySteamConnection.find(info->m_hConn);

	// The connection request already exists
	if (ExistingConnection != IndiciesBySteamConnection.end())
	{
		IndiciesBySteamConnection.modify(
			ExistingConnection, [](Connection &c) { c.SetNewState(ConnectionState::eConnecting); });
		logger.DebugFormatted(
			"Existing connection to ({}) started by me transitioning to "
			"CONNECTING state",
			ExistingConnection->target.ToString());
		return;
	}

	/* ADD CHECK TO CHECK IF THE GIVEN NETWORKING IDENTITY ALREADY EXISTS IN THE
	CONNECTIONS MAP. IMPLYING THAT THE SAME TARGET TRIED TO CONNECT MULTIPLE
	TIMES IN A ROW. CAUSES A CRASH*/

	// ----- New incoming connection path (could be internal or external) -----
	IPAddress address(info->m_info.m_addrRemote);

	int identityByteStreamSize = 0;
	const uint8 *identityByteStream =
		(const uint8 *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);

	// If no identity or malformed, treat as EXTERNAL client (Unity, tools)
	NetworkIdentity ID;
	if (identityByteStream &&
		info->m_info.m_identityRemote.m_eType == k_ESteamNetworkingIdentityType_GenericBytes)
	{
		ByteReader br(std::span(identityByteStream, identityByteStreamSize));

		ID.Deserialize(br);
	}

	// If connection is internal?
	if (ID.IsInternal())
	{
		bool known = false;

		known = ServerRegistry::Get().ExistsInRegistry(ID);

		logger.DebugFormatted(
			"Incoming Internal Connection from: {} at {} (In Server Registry? "
			"{})",
			ID.ToString(), address.ToString(), known ? "yes" : "no");
	}
	else
	{
		logger.DebugFormatted("Incoming External Connection from: {} at {} ", ID.ToString(),
							  address.ToString());
	}
	// Accept the connection
	if (EResult result = networkInterface->AcceptConnection(info->m_hConn); result != k_EResultOK)
	{
		logger.ErrorFormatted("Error accepting connection: reason: {}", uint64(result));
		networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
		return;
	}
	// Register new connection (mark external if not registry-known)
	Connection newCon;
	newCon.SteamConnection = info->m_hConn;
	newCon.SetNewState(ConnectionState::eConnecting);
	newCon.address = address;
	newCon.target = ID;
	newCon.kind = ID.IsInternal() ? ConnectionKind::eInternal : ConnectionKind::eExternal;
	Connections.insert(newCon);
}

void Interlink::CallbackOnClosedByPear(SteamCBInfo info)
{
	auto &bySteam = Connections.get<IndexByHSteamNetConnection>();
	auto it = bySteam.find(info->m_hConn);

	if (it == bySteam.end())
	{
		logger.Error(
			"ClosedByPeer received for unknown connection. This should never "
			"happen");
		return;
	}

	NetworkIdentity closedID = it->target;

	logger.DebugFormatted("Connection closed by peer: {}", closedID.ToString());

	networkInterface->CloseConnection(info->m_hConn, 0, "Connection closed by peer. aka you", true);

	// Remove from internal table BEFORE notifying callbacks.
	bySteam.erase(it);
}

void Interlink::CallbackOnProblemDetectedLocally(SteamCBInfo info)
{
	auto &bySteam = Connections.get<IndexByHSteamNetConnection>();
	auto it = bySteam.find(info->m_hConn);

	if (it == bySteam.end())
	{
		logger.Warning("problem detected locally received for unknown connection.");
		return;
	}

	NetworkIdentity closedID = it->target;

	logger.DebugFormatted("Connection closed by peer: {}", closedID.ToString());

	// Remove from internal table BEFORE notifying callbacks.
	bySteam.erase(it);
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
	logger.Debug("OnConnected");
	auto &indiciesBySteamConn = Connections.get<IndexByHSteamNetConnection>();

	if (auto v = indiciesBySteamConn.find(info->m_hConn); v != indiciesBySteamConn.end())
	{
		indiciesBySteamConn.modify(
			v, [](Connection &c) { c.SetNewState(ConnectionState::eConnected); });
		bool result =
			networkInterface->SetConnectionPollGroup(v->SteamConnection, PollGroup.value());
		if (!result)
		{
			logger.ErrorFormatted("Failed to assign connection from {} to pollgroup",
								  v->target.ToString());
		}

		if (!v->IsInternal())
		{
			logger.DebugFormatted(" - External client Connected from {}", v->address.ToString());
			OnClientConnected(*v);
		}
		else
			logger.DebugFormatted(" - {} Connected", v->target.ToString());

		if (QueuedPacketsOnConnect.contains(v->target) &&
			!QueuedPacketsOnConnect.at(v->target).empty())
		{
			for (const auto &[msg, sendflag] : QueuedPacketsOnConnect.at(v->target))
			{
				SendMessage(v->target, msg, sendflag);
			}
			QueuedPacketsOnConnect.erase(v->target);
		}
	}
	else
	{
		IPAddress address(info->m_info.m_addrRemote);
		logger.ErrorFormatted("FUcking idiot. connection not stored internally. {}",
							  address.ToString(true));
		int identityByteStreamSize = 0;
		const uint8_t *identityByteStream =
			(const uint8_t *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);
		if (identityByteStream)
		{
			auto sp = std::span(identityByteStream, identityByteStreamSize);
			ByteReader br(sp);
			NetworkIdentity id;
			id.Deserialize(br);
			logger.ErrorFormatted("fucking idoit #2. Identity of remote {}", id.ToString());
		}
		ASSERT(false,
			   "Connected? to non existent connection?, HSteamNetConnection "
			   "was not found");
	}
}

void Interlink::OpenListenSocket(PortType port)
{
	SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
			   (void *)SteamNetConnectionStatusChanged);

	SteamNetworkingIPAddr addr;
	addr.SetIPv4(0, 0);
	addr.m_port = port;

	ListeningSocket = networkInterface->CreateListenSocketIP(addr, 1, &opt);
	if (ListeningSocket.value() == k_HSteamListenSocket_Invalid)
	{
		logger.ErrorFormatted("Failed to listen on port {}", port);
		throw std::runtime_error(std::format("Failed to listen on port {}", port));
	}
	logger.DebugFormatted("Opened listen socket on port {}", port);
}

void Interlink::ReceiveMessages()
{
	ISteamNetworkingMessage *pIncomingMessages[32];
	int numMsgs =
		networkInterface->ReceiveMessagesOnPollGroup(PollGroup.value(), pIncomingMessages, 32);

	for (int i = 0; i < numMsgs; ++i)
	{
		ISteamNetworkingMessage *msg = pIncomingMessages[i];

		const Connection &sender = *Connections.get<IndexByHSteamNetConnection>().find(msg->m_conn);

		const void *data = msg->m_pData;
		size_t size = msg->m_cbSize;

		// Normal internal dispatch
		std::span<const uint8_t> span = std::span<const uint8_t>((uint8_t *)data, size);
		const auto packet = PacketRegistry::Get().CreateFromBytes(span);
		//logger.DebugFormatted("Arrived Packet of type {} from {}",
		//					  packet->GetPacketName(), sender.target.ToString());
		packet_manager.Dispatch(*packet, packet->GetPacketType(),
								PacketManager::PacketInfo{.sender = sender.target});

		msg->Release();
	}
}

void Interlink::Init()
{
	logger.Debug("Interlink init");
	ASSERT(NetworkCredentials::Get().GetID().Type != NetworkIdentityType::eInvalid,
		   "Invalid Interlink Type");
	ASSERT(NetworkCredentials::Get().GetID().IsInternal(), "Interlink is for internal only");

	// Single init per process (no repeated warnings)
	if (!EnsureGNSInitialized())
		return;

	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Warning,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
		{ Interlink::Get().OnDebugOutput(eType, pszMsg); });

	networkInterface = SteamNetworkingSockets();

	// Identity setup (unchanged)
	ByteWriter bw;
	NetworkCredentials::Get().GetID().Serialize(bw);
	const auto IdentityByteStream = std::string(bw.as_string_view());
	logger.Debug("Settings Networking Identity");
	SteamNetworkingIdentity identity;
	const bool SetIdentity =
		identity.SetGenericBytes(IdentityByteStream.data(), IdentityByteStream.size());
	ASSERT(SetIdentity, "Failed Identity set");
	networkInterface->ResetIdentity(&identity);
	// Create poll group
	PollGroup = networkInterface->CreatePollGroup();

	// grab public address
	std::optional<std::string> pubIP;
	std::optional<uint32_t> pubPort;
	IPAddress pub;
	OpenListenSocket(_PORT_INTERLINK);

	// registering to database + opening listen sockets
	IPAddress ipAddress;
	if (NetworkCredentials::Get().GetID().Type == NetworkIdentityType::eProxy)
	{
		// Register Demigod in ProxyRegistry
		ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":" +
						std::to_string(_PORT_INTERLINK));
		// ProxyRegistry::Get().RegisterSelf(NetworkCredentials::Get().GetID(), ipAddress);
		ServerRegistry::Get().RegisterSelf(NetworkCredentials::Get().GetID(), ipAddress);

		// Register public address
		// pubIP =
		// DockerIO::Get().GetServiceNodePublicIP(_PROXY_SERVICE_NAME);
		// pubPort = _PORT_PROXY;
		// pub.Parse(*pubIP + ":" + std::to_string(*pubPort));
		// ProxyRegistry::Get().RegisterPublicAddress(NetworkCredentials::Get().GetID(), pub);
		ServerRegistry::Get().RegisterPublicAddress(NetworkCredentials::Get().GetID(), pub);

		logger.DebugFormatted("[Demigod] Public Swarm address = {}", pub.ToString());

		// logger.DebugFormatted("[Interlink]Registered in ProxyRegistry as
		// {}:{}",
		//					   NetworkCredentials::Get().GetID().ToString(), ipAddress.ToString());
	}
	else
	{
		// Register internal (container) address
		IPAddress ipAddress;
		ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":" +
						std::to_string(_PORT_INTERLINK));
		ServerRegistry::Get().RegisterSelf(NetworkCredentials::Get().GetID(), ipAddress);
		logger.DebugFormatted("[Interlink]Registered in ServerRegistry as {}:{}",
							  NetworkCredentials::Get().GetID().ToString(), ipAddress.ToString());
	}

	// Existing post-init behavior (unchanged)
	switch (NetworkCredentials::Get().GetID().Type)
	{
		case NetworkIdentityType::eShard:
		{
			EstablishConnectionTo(NetworkIdentity::MakeIDWatchDog());
			for (const auto &server : ServerRegistry::Get().GetServers())
			{
				if (server.first.Type == NetworkIdentityType::eShard)
				{
					EstablishConnectionTo(server.first);
				}
			}
		}
		break;

		case NetworkIdentityType::eProxy:
		{
			EstablishConnectionTo(NetworkIdentity::MakeIDWatchDog());
		}
		break;

		case NetworkIdentityType::eCartograph:
		{
			EstablishConnectionTo(NetworkIdentity::MakeIDWatchDog());
		}
		break;
		case NetworkIdentityType::eWatchDog:
		default:
			break;
	}

	TickThread = std::jthread(
		[this](std::stop_token st)
		{
			using clock = std::chrono::steady_clock;
			auto last = clock::now();

			while (!st.stop_requested())
			{
				Tick();

				auto now = clock::now();
				if (now - last < std::chrono::milliseconds(2))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}
				last = now;
			}
		});
	IsInit = true;
}

void Interlink::Shutdown()
{
	CloseAllConnections();
	TickThread.request_stop();
	TickThread.join();
	logger.Debug("Interlink Shutdown");
}

bool Interlink::EstablishConnectionAtIP(const NetworkIdentity &id, const IPAddress &address)
{
	// return EstablishConnectionTo(InterLinkIdentifier::MakeIDGod());
	if (Connections.get<IndexByTarget>().contains(id))
	{
		const Connection &existing = *Connections.get<IndexByTarget>().find(id);
		if (existing.state == ConnectionState::eConnected)
		{
			logger.WarningFormatted("Connection to {} already established", id.ToString());
			return true;
		}
		else if (existing.state == ConnectionState::eConnecting ||
				 existing.state == ConnectionState::ePreConnecting)
		{
			logger.WarningFormatted("Already connecting to {}", id.ToString());
			return true;
		}
	}

	// Step 1: Build God ID
	NetworkIdentity godID = NetworkIdentity::MakeIDWatchDog();

	// Step 2: Try to get public IP:port from registry
	// std::optional<IPAddress> publicAddr =
	// ServerRegistry::Get().GetPublicAddress(godID); if
	// (!publicAddr.has_value())
	//{
	//  logger.ErrorFormatted("No public address found for {} in Server
	//  Registry", godID.ToString()); return false;
	//}
	// else
	//{
	//  std::cout << "God is reachable at " << publicAddr->ToString() <<
	//  std::endl;
	//}

	// auto IP = ServerRegistry::Get().GetIPOfID(id);

	Connection conn;
	conn.address = address;
	conn.target = id;
	// conn.address = *publicAddr;
	// conn.target = godID;
	conn.SetNewState(ConnectionState::ePreConnecting);
	conn.kind = ConnectionKind::eExternal;	// external connection path (direct IP)
	Connections.insert(conn);

	logger.DebugFormatted("Establishing direct connection to {} at {}", id.ToString(),
						  address.ToString());
	return true;
}

bool Interlink::EstablishConnectionTo(const NetworkIdentity &id)
{
	ASSERT(NetworkCredentials::Get().GetID().Type != NetworkIdentityType::eGameClient,
		   "Game client must use the ip one");
	// Prevent duplicate attempts
	if (Connections.get<IndexByTarget>().contains(id))
	{
		const Connection &existing = *Connections.get<IndexByTarget>().find(id);
		if (existing.state == ConnectionState::eConnected)
		{
			logger.WarningFormatted("Connection to {} already established", id.ToString());
			return true;
		}
		if (existing.state == ConnectionState::eConnecting ||
			existing.state == ConnectionState::ePreConnecting)
		{
			logger.WarningFormatted("Already connecting to {}", id.ToString());
			return true;
		}
	}
	// ---------------------------------------------------------------
	// INTERNAL: must exist in ServerRegistry
	// ---------------------------------------------------------------
	if (id.IsInternal())
	{
		auto IP = ServerRegistry::Get().GetIPOfID(id);

		for (int i = 0; i < 5 && !IP.has_value(); i++)
		{
			logger.ErrorFormatted(
				"IP not found for {} in Server Registry. Trying again in 1 "
				"second",
				id.ToString());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			IP = ServerRegistry::Get().GetIPOfID(id);
		}

		if (!IP.has_value())
		{
			logger.ErrorFormatted(
				"Failed to establish connection after 5 tries. IP not found in "
				"Server Registry {}",
				id.ToString());
			logger.DebugFormatted("All entries in Server Registry:");
			for (const auto &[SID, Entry] : ServerRegistry::Get().GetServers())
			{
				logger.DebugFormatted(" - {} at {}", Entry.identifier.ToString(),
									  Entry.address.ToString());
			}

			return false;  // no assert crash, just graceful fail
		}

		Connection conn;
		conn.address = IP.value();
		conn.target = id;
		conn.SetNewState(ConnectionState::ePreConnecting);
		Connections.insert(conn);

		logger.DebugFormatted("Establishing internal connection to {}", id.ToString());
		return true;
	}

	// ---------------------------------------------------------------
	// EXTERNAL: skip registry (for now, nothing directly connects to clients)
	// ---------------------------------------------------------------
	if (id.Type == NetworkIdentityType::eGameClient)
	{
		// For now, external clients are expected to connect INBOUND to God.
		// Outbound from inside to a GameClient is not required.
		logger.WarningFormatted("Skipping registry lookup for external client {}",
								NetworkCredentials::Get().GetID().ToString());
		return true;
	}

	// Default case (unknown type)
	logger.WarningFormatted("Unknown interlink type for {} - skipping connection",
							NetworkCredentials::Get().GetID().ToString());
	return false;
}

void Interlink::CloseConnectionTo(const NetworkIdentity &id, int reason, const char *debug)
{
	auto &byTarget = Connections.get<IndexByTarget>();

	auto it = byTarget.find(id);
	if (it == byTarget.end())
	{
		logger.WarningFormatted("CloseConnectionTo: No active connection found for {}",
								id.ToString());
		return;
	}

	HSteamNetConnection conn = it->SteamConnection;

	logger.DebugFormatted("Closing connection to {} (SteamConn={})", id.ToString(), (uint64)conn);

	// Close on the networking side
	networkInterface->CloseConnection(conn, reason, debug, false);

	// Remove from table
	byTarget.erase(it);
}
/*
void Interlink::DebugPrint()
{
	for (const auto &connection : Connections)
	{
		logger.Debug(connection.ToString());
	}
}*/

void Interlink::Tick()
{
	GenerateNewConnections();
	ReceiveMessages();
	networkInterface->RunCallbacks();  // process events
}

void Interlink::GetConnectionTelemetry(std::vector<ConnectionTelemetry> &out)
{
	out.clear();

	ISteamNetworkingSockets *sockets = SteamNetworkingSockets();
	if (!sockets)
		return;

	const auto &byState = Connections.get<IndexByState>();

	auto [it, end] = byState.equal_range(ConnectionState::eConnected);
	for (; it != end; ++it)
	{
		const Connection &conn = *it;

		SteamNetConnectionRealTimeStatus_t status{};
		sockets->GetConnectionRealTimeStatus(conn.SteamConnection, &status, 0, nullptr);

		ConnectionTelemetry t{};
		t.pingMs = status.m_nPing;
		t.inBytesPerSec = status.m_flInBytesPerSec;
		t.outBytesPerSec = status.m_flOutBytesPerSec;
		t.inPacketsPerSec = status.m_flInPacketsPerSec;

		t.pendingReliableBytes = status.m_cbPendingReliable;
		t.pendingUnreliableBytes = status.m_cbPendingUnreliable;
		t.sentUnackedReliableBytes = status.m_cbSentUnackedReliable;

		t.queueTimeUsec = status.m_usecQueueTime;

		t.qualityLocal = status.m_flConnectionQualityLocal;
		t.qualityRemote = status.m_flConnectionQualityRemote;
		t.state = status.m_eState;

		t.IdentityId = NetworkCredentials::Get().GetID().ToString();
		t.targetId = conn.target.ToString();

		out.push_back(std::move(t));
	}
}
void Interlink::OnClientConnected(const Connection &c)
{
	ASSERT(c.target.Type == NetworkIdentityType::eGameClient, "Invalid Target");
	if (c.target.ID.is_nil())
	{
		NetworkIdentity newIdentity = c.target;
		newIdentity.ID = UUIDGen::Gen();
		logger.DebugFormatted("Client Connection with no ID.\n	Assigning {}",
							  newIdentity.ToString());

		ClientIDAssignPacket IDAssignPacket;
		IDAssignPacket.AssignedClientID = newIdentity;
		Connections.get<IndexByHSteamNetConnection>().modify(
			Connections.get<IndexByHSteamNetConnection>().find(c.SteamConnection),
			[newIdentity = newIdentity](Connection &C) { C.target = newIdentity; });

		SendMessage(newIdentity, IDAssignPacket, NetworkMessageSendFlag::eReliableNow);
		Client client;
		client.ID = newIdentity.ID;
		client.ip = c.address;
		ClientManifest::Get().RegisterClient(client);
		ClientManifest::Get().AssignProxyClient(client.ID, NetworkCredentials::Get().GetID());

		ClientConnectEvent cce;
		cce.client = client;
		cce.ConnectedProxy = NetworkCredentials::Get().GetID();
		EventSystem::Get().Dispatch(cce);

		HandshakeService::Get().OnClientConnect(client);
	}
}
void Interlink::CloseAllConnections(int reason) {}

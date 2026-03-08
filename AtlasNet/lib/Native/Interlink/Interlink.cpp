#include "Interlink.hpp"

#include <steam/isteamnetworkingsockets.h>
#include <steam/steamtypes.h>

#include <atomic>
#include <boost/describe/enum_to_string.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <stop_token>
#include <string_view>
#include <thread>

// #include "Database/ProxyRegistry.hpp"
#include "Client/Client.hpp"
#include "Client/Database/ClientManifest.hpp"
#include "Database/NodeManifest.hpp"
#include "Database/ServerRegistry.hpp"
#include "Docker/DockerIO.hpp"
#include "Events/GlobalEvents.hpp"
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

namespace
{
std::optional<std::string> GetNonEmptyEnv(const char* key)
{
	const char* value = std::getenv(key);
	if (!value || !*value)
	{
		return std::nullopt;
	}
	return std::string(value);
}

std::string GetJsonString(const nlohmann::json& object, const char* key)
{
	if (!object.is_object())
	{
		return {};
	}
	const auto it = object.find(key);
	if (it == object.end() || !it->is_string())
	{
		return {};
	}
	return it->get<std::string>();
}

std::string StripLeadingSlashes(std::string value)
{
	while (!value.empty() && value.front() == '/')
	{
		value.erase(value.begin());
	}
	return value;
}

struct SwarmPlacementInfo
{
	std::string nodeName;
	std::string taskName;
};

std::optional<SwarmPlacementInfo> ResolveSwarmPlacementInfoFromDocker()
{
	const auto self = DockerIO::Get().InspectSelf();
	if (!self.is_object())
	{
		return std::nullopt;
	}

	SwarmPlacementInfo out;
	if (const auto configIt = self.find("Config");
		configIt != self.end() && configIt->is_object())
	{
		const auto& config = *configIt;
		if (const auto labelsIt = config.find("Labels");
			labelsIt != config.end() && labelsIt->is_object())
		{
			const auto& labels = *labelsIt;
			out.taskName = GetJsonString(labels, "com.docker.swarm.task.name");
			const std::string nodeId = GetJsonString(labels, "com.docker.swarm.node.id");
			if (!nodeId.empty())
			{
				const std::string nodeResponse = DockerIO::Get().request("GET", "/nodes/" + nodeId);
				const auto node = nlohmann::json::parse(nodeResponse, nullptr, false);
				if (node.is_object())
				{
					if (const auto descIt = node.find("Description");
						descIt != node.end() && descIt->is_object())
					{
						out.nodeName = GetJsonString(*descIt, "Hostname");
					}
					if (out.nodeName.empty())
					{
						if (const auto specIt = node.find("Spec");
							specIt != node.end() && specIt->is_object())
						{
							out.nodeName = GetJsonString(*specIt, "Name");
						}
					}
				}
				if (out.nodeName.empty())
				{
					out.nodeName = nodeId;
				}
			}
		}
	}

	if (out.taskName.empty())
	{
		out.taskName = StripLeadingSlashes(GetJsonString(self, "Name"));
	}
	if (out.nodeName.empty())
	{
		if (const auto nodeIt = self.find("Node");
			nodeIt != self.end() && nodeIt->is_object())
		{
			out.nodeName = GetJsonString(*nodeIt, "Name");
		}
	}

	if (out.nodeName.empty() && out.taskName.empty())
	{
		return std::nullopt;
	}
	return out;
}

std::string ResolveInterlinkAdvertiseIp()
{
	if (const auto explicitIp = GetNonEmptyEnv("INTERLINK_ADVERTISE_IP");
		explicitIp.has_value())
	{
		return *explicitIp;
	}
	if (const auto podIp = GetNonEmptyEnv("POD_IP"); podIp.has_value())
	{
		return *podIp;
	}
	return DockerIO::Get().GetSelfContainerIP();
}

IPAddress BuildInterlinkAddress(const std::string& ip, const PortType port)
{
	IPAddress address;
	address.Parse(ip + ":" + std::to_string(port));
	return address;
}
}  // namespace

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

	auto QueueMessageOnConnect = [&]() {
		_WriteLock([&]()
				   { QueuedPacketsOnConnect[who].push_back(std::make_pair(packet, sendFlag)); });
	};

	bool contains = _ReadLock([&]() { return Connections.get<IndexByTarget>().contains(who); });

	if (!contains)
	{
		logger.DebugFormatted("Connection to \"{}\" was not established, connecting...",
							  who.ToString());
		EstablishConnectionTo(who);
		QueueMessageOnConnect();
		return;
	}

	// Read the connection state and SteamConnection under read lock,
	// then operate outside the lock.
	ConnectionState state;
	HSteamNetConnection steamConn = k_HSteamNetConnection_Invalid;
	bool found = _ReadLock(
		[&]() -> bool
		{
			auto &index = Connections.get<IndexByTarget>();
			auto it = index.find(who);
			if (it == index.end())
				return false;
			state = it->state;
			steamConn = it->SteamConnection;
			return true;
		});

	if (!found)
	{
		// race: connection disappeared after earlier check; queue and attempt reconnect
		logger.DebugFormatted("Connection {} disappeared, queuing", who.ToString());
		EstablishConnectionTo(who);
		QueueMessageOnConnect();
		return;
	}

	if (state != ConnectionState::eConnected)
	{
		if (state == ConnectionState::ePreConnecting || state == ConnectionState::eConnecting)
		{
			logger.DebugFormatted("Connection to {} is {} , queuing message...", who.ToString(),
								  boost::describe::enum_to_string(state, "unknown"));
			QueueMessageOnConnect();
			return;
		}
		else
		{
			logger.DebugFormatted("Connection to {} state is {} , connecting...", who.ToString(),
								  boost::describe::enum_to_string(state, "unknown"));
			EstablishConnectionTo(who);
			QueueMessageOnConnect();
			return;
		}
	}

	// At this point we have a connected steamConn; do serialization and sending outside locks.
	ByteWriter bw;
	packet->Serialize(bw);
	const auto data_span = bw.bytes();

	const auto SendResult = networkInterface->SendMessageToConnection(
		steamConn, data_span.data(), data_span.size_bytes(), (int)sendFlag, nullptr);

	if (SendResult != k_EResultOK)
	{
		logger.ErrorFormatted(
			"Unable to send message. SteamNetworkingSockets returned "
			"result code {}",
			(int)SendResult);
	}
}

void Interlink::GenerateNewConnections()
{
	// 1) Collect a snapshot of pre-connecting targets and their addresses
	std::vector<std::pair<NetworkIdentity, IPAddress>> toConnect;
	_ReadLock(
		[&]()
		{
			auto &byState = Connections.get<IndexByState>();
			auto range = byState.equal_range(ConnectionState::ePreConnecting);
			for (auto it = range.first; it != range.second; ++it)
			{
				toConnect.emplace_back(it->target, it->address);
			}
		});

	if (toConnect.empty())
		return;

	SteamNetworkingConfigValue_t opt[1];
	opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				  (void *)SteamNetConnectionStatusChanged);

	// 2) For each, perform connect (network call) outside the lock, then update entry under write
	// lock
	for (auto &p : toConnect)
	{
		const auto &target = p.first;
		const auto &addr = p.second;

		logger.DebugFormatted("Connecting to {} at {}", target.ToString(), addr.ToString());
		HSteamNetConnection conn =
			networkInterface->ConnectByIPAddress(addr.ToSteamIPAddr(), 1, opt);

		if (conn == k_HSteamNetConnection_Invalid)
		{
			logger.ErrorFormatted("Failed to Generate New Connection {}", addr.ToString());
			continue;
		}

		// write-back connection handle into Connections if it still exists and still preconnecting
		_WriteLock(
			[&]()
			{
				auto &indexTarget = Connections.get<IndexByTarget>();
				auto it = indexTarget.find(target);
				if (it != indexTarget.end() && it->state == ConnectionState::ePreConnecting)
				{
					// modify using IndexByTarget (or find then modify by stable iterator via global
					// index).
					indexTarget.modify(it, [conn](Connection &c) { c.SteamConnection = conn; });
				}
				else
				{
					// entry vanished or changed; close the connection we just opened
					networkInterface->CloseConnection(conn, 0, "Stale after connect", false);
				}
			});
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

	// Find existing under read lock
	bool exists = _ReadLock(
		[&]() {
			return IndiciesBySteamConnection.find(info->m_hConn) != IndiciesBySteamConnection.end();
		});

	if (exists)
	{
		_WriteLock(
			[&]()
			{
				auto it = IndiciesBySteamConnection.find(info->m_hConn);
				if (it != IndiciesBySteamConnection.end())
					IndiciesBySteamConnection.modify(
						it, [](Connection &c) { c.SetNewState(ConnectionState::eConnecting); });
			});
		logger.DebugFormatted("Existing connection started by me -> CONNECTING");
		return;
	}

	// Extract identity
	IPAddress address(info->m_info.m_addrRemote);

	int identityByteStreamSize = 0;
	const uint8 *identityByteStream =
		(const uint8 *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);

	NetworkIdentity ID;
	if (identityByteStream &&
		info->m_info.m_identityRemote.m_eType == k_ESteamNetworkingIdentityType_GenericBytes)
	{
		ByteReader br(std::span(identityByteStream, identityByteStreamSize));
		ID.Deserialize(br);
	}

	// Accept the connection before inserting
	if (EResult result = networkInterface->AcceptConnection(info->m_hConn); result != k_EResultOK)
	{
		logger.ErrorFormatted("Error accepting connection: reason: {}", uint64(result));
		networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
		return;
	}

	Connection newCon;
	newCon.SteamConnection = info->m_hConn;
	newCon.SetNewState(ConnectionState::eConnecting);
	newCon.address = address;
	newCon.target = ID;
	newCon.kind = ID.IsInternal() ? ConnectionKind::eInternal : ConnectionKind::eExternal;

	// Insert under write lock
	_WriteLock([&]() { Connections.insert(newCon); });
}

void Interlink::CallbackOnClosedByPear(SteamCBInfo info)
{
	auto &bySteam = Connections.get<IndexByHSteamNetConnection>();

	// find + erase under write lock
	NetworkIdentity closedID;
	bool found = _ReadLock([&]() { return bySteam.find(info->m_hConn) != bySteam.end(); });

	if (!found)
	{
		logger.Error("ClosedByPeer received for unknown connection. This should never happen");
		return;
	}

	_WriteLock(
		[&]()
		{
			auto it = bySteam.find(info->m_hConn);
			if (it == bySteam.end())
			{
				// double-checked
				return;
			}
			closedID = it->target;
			logger.DebugFormatted("Connection closed by peer: {}", closedID.ToString());
			networkInterface->CloseConnection(info->m_hConn, 0,
											  "Connection closed by peer. aka you", true);
			bySteam.erase(it);
		});
}

void Interlink::CallbackOnProblemDetectedLocally(SteamCBInfo info)
{
	auto &bySteam = Connections.get<IndexByHSteamNetConnection>();

	bool exists = _ReadLock([&]() { return bySteam.find(info->m_hConn) != bySteam.end(); });
	if (!exists)
	{
		logger.Warning("problem detected locally received for unknown connection.");
		return;
	}

	_WriteLock(
		[&]()
		{
			auto it = bySteam.find(info->m_hConn);
			if (it != bySteam.end())
			{
				NetworkIdentity closedID = it->target;
				logger.DebugFormatted("Connection problem detected locally for: {}",
									  closedID.ToString());
				bySteam.erase(it);
			}
		});
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
	logger.Debug("OnConnected");
	auto &indiciesBySteamConn = Connections.get<IndexByHSteamNetConnection>();

	// find under read lock
	bool found = _ReadLock(
		[&]() { return indiciesBySteamConn.find(info->m_hConn) != indiciesBySteamConn.end(); });

	if (!found)
	{
		// no internal record; log and assert like before (unchanged)
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
		return;
	}

	// Change state and get copy of target for further processing (and queued messages)
	NetworkIdentity target;
	bool isInternal = false;
	_WriteLock(
		[&]()
		{
			auto v = indiciesBySteamConn.find(info->m_hConn);
			if (v != indiciesBySteamConn.end())
			{
				indiciesBySteamConn.modify(
					v, [](Connection &c) { c.SetNewState(ConnectionState::eConnected); });
				target = v->target;
				isInternal = v->IsInternal();
			}
		});

	// assign to pollgroup (call outside locks)
	bool result = networkInterface->SetConnectionPollGroup(info->m_hConn, PollGroup.value());
	if (!result)
	{
		logger.ErrorFormatted("Failed to assign connection from {} to pollgroup",
							  target.ToString());
	}

	if (!isInternal)
	{
		logger.DebugFormatted(" - External client Connected from {}", target.ToString());
		// call OnClientConnected which modifies connections; do it safely
		Connection c = _ReadLock(
			[&]() { return *Connections.get<IndexByHSteamNetConnection>().find(info->m_hConn); });
		OnClientConnected(c);
	}
	else
	{
		logger.DebugFormatted(" - {} Connected", target.ToString());
	}

	// deliver queued messages: move out queued messages under write lock, then release and send
	std::vector<std::pair<std::shared_ptr<IPacket>, NetworkMessageSendFlag>> queued;
	_WriteLock(
		[&]()
		{
			auto it = QueuedPacketsOnConnect.find(target);
			if (it != QueuedPacketsOnConnect.end() && !it->second.empty())
			{
				queued = std::move(it->second);
				QueuedPacketsOnConnect.erase(it);
			}
		});

	for (const auto &p : queued)
	{
		SendMessage(target, p.first, p.second);
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

		// find sender under read-lock and copy its identity
		NetworkIdentity senderIdentity;
		{
			_ReadLock(
				[&]()
				{
					auto it = Connections.get<IndexByHSteamNetConnection>().find(msg->m_conn);
					if (it != Connections.get<IndexByHSteamNetConnection>().end())
						senderIdentity = it->target;
				});
		}

		const void *data = msg->m_pData;
		size_t size = msg->m_cbSize;

		std::span<const uint8_t> span = std::span<const uint8_t>((uint8_t *)data, size);
		const auto packet = PacketRegistry::Get().CreateFromBytes(span); 

		// Dispatch: we pass the sender we captured
		packet_manager.Dispatch(*packet, packet->GetPacketType(),
								PacketManager::PacketInfo{.sender = senderIdentity});

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
	OpenListenSocket(_PORT_INTERLINK);

	// registering to database + opening listen sockets
	const std::string advertiseIp = ResolveInterlinkAdvertiseIp();
	if (advertiseIp.empty())
	{
		logger.Error(
			"[Interlink] Could not resolve local advertise IP (env INTERLINK_ADVERTISE_IP/POD_IP or eth0).");
		return;
	}
	IPAddress ipAddress = BuildInterlinkAddress(advertiseIp, _PORT_INTERLINK);
	logger.DebugFormatted("[Interlink] Advertising address {}", ipAddress.ToString());
	if (NetworkCredentials::Get().GetID().Type == NetworkIdentityType::eProxy)
	{
		ServerRegistry::Get().RegisterSelf(NetworkCredentials::Get().GetID(), ipAddress);

		IPAddress publicAddress = ipAddress;
		if (const auto publicIp = GetNonEmptyEnv("INTERLINK_PUBLIC_IP");
			publicIp.has_value())
		{
			uint32_t publicPort = _PORT_PROXY;
			if (const auto publicPortText = GetNonEmptyEnv("INTERLINK_PUBLIC_PORT");
				publicPortText.has_value())
			{
				try
				{
					publicPort = static_cast<uint32_t>(std::stoul(*publicPortText));
				}
				catch (...)
				{
					publicPort = _PORT_PROXY;
				}
			}
			publicAddress = BuildInterlinkAddress(*publicIp, static_cast<PortType>(publicPort));
		}
		ServerRegistry::Get().RegisterPublicAddress(NetworkCredentials::Get().GetID(),
													publicAddress);
		logger.DebugFormatted("[Interlink] Registered proxy public address {}",
							  publicAddress.ToString());
	}
	else
	{
		ServerRegistry::Get().RegisterSelf(NetworkCredentials::Get().GetID(), ipAddress);
		logger.DebugFormatted("[Interlink]Registered in ServerRegistry as {}:{}",
							  NetworkCredentials::Get().GetID().ToString(), ipAddress.ToString());
	}

	if (NetworkCredentials::Get().GetID().Type == NetworkIdentityType::eShard)
	{
		NodeManifestEntry entry;
		if (const auto nodeName = GetNonEmptyEnv("NODE_NAME"); nodeName.has_value())
		{
			entry.nodeName = *nodeName;
		}
		if (const auto podName = GetNonEmptyEnv("POD_NAME"); podName.has_value())
		{
			entry.podName = *podName;
		}
		if (const auto podIp = GetNonEmptyEnv("POD_IP"); podIp.has_value())
		{
			entry.podIP = *podIp;
		}
		else
		{
			entry.podIP = advertiseIp;
		}
		if (entry.nodeName.empty() || entry.podName.empty())
		{
			try
			{
				if (const auto swarmPlacement = ResolveSwarmPlacementInfoFromDocker();
					swarmPlacement.has_value())
				{
					if (entry.nodeName.empty() && !swarmPlacement->nodeName.empty())
					{
						entry.nodeName = swarmPlacement->nodeName;
					}
					if (entry.podName.empty() && !swarmPlacement->taskName.empty())
					{
						entry.podName = swarmPlacement->taskName;
					}
				}
			}
			catch (const std::exception& ex)
			{
				logger.DebugFormatted("[Interlink] Failed to resolve swarm placement metadata: {}",
									  ex.what());
			}
			catch (...)
			{
				logger.Debug("[Interlink] Failed to resolve swarm placement metadata.");
			}
		}

		NodeManifest::Get().RegisterShardNode(NetworkCredentials::Get().GetID(), entry);
	}

	// Existing post-init behavior (unchanged)
	switch (NetworkCredentials::Get().GetID().Type)
	{
		case NetworkIdentityType::eShard:
		{
			EstablishConnectionTo(NetworkIdentity::MakeIDWatchDog());
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
			while (!st.stop_requested())
			{
				Tick();

				std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
	// quick check under read lock
	bool existsAndConnected = _ReadLock(
		[&]() -> bool
		{
			auto &idx = Connections.get<IndexByTarget>();
			auto it = idx.find(id);
			if (it == idx.end())
				return false;
			if (it->state == ConnectionState::eConnected)
				return true;
			if (it->state == ConnectionState::eConnecting ||
				it->state == ConnectionState::ePreConnecting)
				return true;
			return false;
		});

	if (existsAndConnected)
	{
		logger.WarningFormatted("Connection to {} already exists/connecting", id.ToString());
		return true;
	}

	Connection conn;
	conn.address = address;
	conn.target = id;
	conn.SetNewState(ConnectionState::ePreConnecting);
	conn.kind = ConnectionKind::eExternal;

	_WriteLock([&]() { Connections.insert(conn); });

	logger.DebugFormatted("Establishing direct connection to {} at {}", id.ToString(),
						  address.ToString());
	return true;
}

bool Interlink::EstablishConnectionTo(const NetworkIdentity &id)
{
	ASSERT(NetworkCredentials::Get().GetID().Type != NetworkIdentityType::eGameClient,
		   "Game client must use the ip one");

	// quick read-lock check
	bool already = _ReadLock(
		[&]() -> bool
		{
			auto &idx = Connections.get<IndexByTarget>();
			auto it = idx.find(id);
			if (it == idx.end())
				return false;
			if (it->state == ConnectionState::eConnected)
				return true;
			if (it->state == ConnectionState::eConnecting ||
				it->state == ConnectionState::ePreConnecting)
				return true;
			return false;
		});

	if (already)
	{
		logger.WarningFormatted("Connection to {} already established or in progress",
								id.ToString());
		return true;
	}

	if (id.IsInternal())
	{
		auto IP = ServerRegistry::Get().GetIPOfID(id);

		for (int i = 0; i < 5 && !IP.has_value(); i++)
		{
			logger.ErrorFormatted(
				"IP not found for {} in Server Registry. Trying again in 1 second", id.ToString());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			IP = ServerRegistry::Get().GetIPOfID(id);
		}

		if (!IP.has_value())
		{
			logger.ErrorFormatted(
				"Failed to establish connection after 5 tries. IP not found in Server Registry {}",
				id.ToString());
			logger.DebugFormatted("All entries in Server Registry:");
			for (const auto &[SID, Entry] : ServerRegistry::Get().GetServers())
			{
				logger.DebugFormatted(" - {} at {}", Entry.identifier.ToString(),
									  Entry.address.ToString());
			}

			return false;
		}

		Connection conn;
		conn.address = IP.value();
		conn.target = id;
		conn.SetNewState(ConnectionState::ePreConnecting);

		_WriteLock([&]() { Connections.insert(conn); });

		logger.DebugFormatted("Establishing internal connection to {}", id.ToString());
		return true;
	}

	if (id.Type == NetworkIdentityType::eGameClient)
	{
		logger.WarningFormatted("Skipping registry lookup for external client {}",
								NetworkCredentials::Get().GetID().ToString());
		return true;
	}

	logger.WarningFormatted("Unknown interlink type for {} - skipping connection",
							NetworkCredentials::Get().GetID().ToString());
	return false;
}

void Interlink::CloseConnectionTo(const NetworkIdentity &id, int reason, const char *debug)
{
	auto &byTarget = Connections.get<IndexByTarget>();

	// find under read lock
	bool found = _ReadLock([&]() { return byTarget.find(id) != byTarget.end(); });

	if (!found)
	{
		logger.WarningFormatted("CloseConnectionTo: No active connection found for {}",
								id.ToString());
		return;
	}

	HSteamNetConnection conn = k_HSteamNetConnection_Invalid;

	_WriteLock(
		[&]()
		{
			auto it = byTarget.find(id);
			if (it == byTarget.end())
				return;
			conn = it->SteamConnection;
			logger.DebugFormatted("Closing connection to {} (SteamConn={})", id.ToString(),
								  (uint64)conn);
			byTarget.erase(it);
		});

	// Close on the networking side outside the lock
	if (conn != k_HSteamNetConnection_Invalid)
	{
		networkInterface->CloseConnection(conn, reason, debug, false);
	}
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

	// Snapshot list of connected connections under read lock
	std::vector<Connection> conns;
	_ReadLock(
		[&]()
		{
			auto &byState = Connections.get<IndexByState>();
			auto range = byState.equal_range(ConnectionState::eConnected);
			for (auto it = range.first; it != range.second; ++it) conns.push_back(*it);
		});

	for (const auto &conn : conns)
	{
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
		logger.DebugFormatted("Client Connection with no ID.\n    Assigning {}",
							  newIdentity.ToString());

		ClientIDAssignPacket IDAssignPacket;
		IDAssignPacket.AssignedClientID = newIdentity;

		// Modify the connection target under write lock
		logger.DebugFormatted("Getting write lock");
		_WriteLock(
			[&]()
			{
				logger.DebugFormatted("Got write lock");

				auto &bySteam = Connections.get<IndexByHSteamNetConnection>();
				auto it = bySteam.find(c.SteamConnection);
				if (it != bySteam.end())
				{
					bySteam.modify(it, [newIdentity](Connection &C) { C.target = newIdentity; });
				}
			});

		SendMessage(newIdentity, IDAssignPacket, NetworkMessageSendFlag::eReliableNow);
		logger.DebugFormatted("Sent ID Assign packet");
		Client client;
		client.ID = newIdentity.ID;
		client.ip = c.address;
		ClientManifest::Get().InsertClient(client);

		ClientHandshakeEvent cce;
		cce.client = client;
		cce.ConnectedProxy = NetworkCredentials::Get().GetID();
		GlobalEvents::Get().Dispatch(cce);

		HandshakeService::Get().OnClientConnect(client);
	}
}
void Interlink::CloseAllConnections(int reason) {}

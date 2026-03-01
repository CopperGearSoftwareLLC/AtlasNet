#include "ClientLink.hpp"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include <stdexcept>
#include <stop_token>
#include <thread>

#include "Client/Client.hpp"
#include "Client/ClientCredentials.hpp"
#include "Global/pch.hpp"
#include "Network/Connection.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"

static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *info)
{
	ClientLink::Get().OnSteamNetConnectionStatusChanged(info);
}

void ClientLink::OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	switch (pInfo->m_info.m_eState)
	{
		case k_ESteamNetworkingConnectionState_Connecting:
			logger.Debug("k_ESteamNetworkingConnectionState_Connecting");

			break;
		case k_ESteamNetworkingConnectionState_Connected:

			logger.Debug("k_ESteamNetworkingConnectionState_Connected");
			OnConnected(pInfo);
			break;
		case k_ESteamNetworkingConnectionState_ClosedByPeer:

			logger.Debug("k_ESteamNetworkingConnectionState_ClosedByPeer");
			break;
		case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:

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
void ClientLink::ConnectToAtlasNet(const IPAddress &address)
{
	SteamNetworkingConfigValue_t opt[1];
	opt[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
				  (void *)SteamNetConnectionStatusChanged);

	HSteamNetConnection conn =
		SteamNetworkingSockets()->ConnectByIPAddress(address.ToSteamIPAddr(), 1, opt);

	bool result = SteamNetworkingSockets()->SetConnectionPollGroup(conn, poll_group);
	if (!result)
	{
		logger.ErrorFormatted("Failed to assign connection from {} to pollgroup",
							  address.ToString());
	}
	Connection c;
	c.SteamConnection = conn;
	c.address = address;
	c.SetNewState(ConnectionState::eConnecting);
	c.target = NetworkIdentityType::eAtlasNetInitial;

	Connections.insert(c);
}
void ClientLink::Init()
{
	InitGNS();
	ByteWriter bw;
	NetworkIdentity myBaseID;
	myBaseID.Type = NetworkIdentityType::eGameClient;
	myBaseID.ID = UUID();
	myBaseID.Serialize(bw);
	const auto IdentityByteStream = std::string(bw.as_string_view());
	logger.Debug("Settings Networking Identity");
	SteamNetworkingIdentity identity;
	const bool SetIdentity =
		identity.SetGenericBytes(IdentityByteStream.data(), IdentityByteStream.size());
	ASSERT(SetIdentity, "Failed Identity set");
	SteamNetworkingSockets()->ResetIdentity(&identity);
	SteamNetworkingUtils()->SetDebugOutputFunction(
		k_ESteamNetworkingSocketsDebugOutputType_Warning,
		[](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
		{ std::cout << "[ClientLink] " << pszMsg << std::endl; });
	poll_group = SteamNetworkingSockets()->CreatePollGroup();
	TickThread = std::jthread(
		[this](std::stop_token st)
		{
			using clock = std::chrono::steady_clock;
			auto last = clock::now();

			while (!st.stop_requested())
			{
				Update();

				auto now = clock::now();
				if (now - last < std::chrono::milliseconds(2))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}
				last = now;
			}
		});
	ClientIDAssignSub = packet_manager.Subscribe<ClientIDAssignPacket>(
		[&](const ClientIDAssignPacket &clientIDPacket, const PacketManager::PacketInfo &info)
		{ OnClientIDAssignedPacket(clientIDPacket, info); });
}
void ClientLink::OnClientIDAssignedPacket(const ClientIDAssignPacket &clientIDPacket,
										  const PacketManager::PacketInfo &info)
{
	logger.DebugFormatted("Received packet assigning ID to {} ",
						  clientIDPacket.AssignedClientID.ToString());
	ClientID id = clientIDPacket.AssignedClientID.ID;
	ClientCredentials::Make(id);
}
void ClientLink::ReceiveMessages()
{
	ISteamNetworkingMessage *pIncomingMessages[32];
	int numMsgs =
		SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(poll_group, pIncomingMessages, 32);

	for (int i = 0; i < numMsgs; ++i)
	{
		ISteamNetworkingMessage *msg = pIncomingMessages[i];

		auto &BySteamCon = Connections.get<IndexByHSteamNetConnection>();
		ASSERT(BySteamCon.contains(msg->m_conn), "Received message from unregistered connection?");
		const auto it = BySteamCon.find(msg->m_conn);

		const Connection &connection = *it;

		const void *data = msg->m_pData;
		size_t size = msg->m_cbSize;

		// Normal internal dispatch
		std::span<const uint8_t> span = std::span<const uint8_t>((uint8_t *)data, size);
		const auto packet = PacketRegistry::Get().CreateFromBytes(span);
		logger.DebugFormatted("Arrived Packet of type {}. Dispatching...", packet->GetPacketName());
		packet_manager.Dispatch(*packet, packet->GetPacketType(),
								PacketManager::PacketInfo{.sender = connection.target});

		msg->Release();
	}
}
void ClientLink::InitGNS()
{
	SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg))
	{
		std::cerr << (std::string("GameNetworkingSockets_Init failed: ") + errMsg) << std::endl;
		throw std::runtime_error("fcukc");
	}
}
void ClientLink::Update()
{
	SteamNetworkingSockets()->RunCallbacks();
	ReceiveMessages();
}
void ClientLink::OnConnected(SteamNetConnectionStatusChangedCallback_t *pInfo)
{
	auto &bySteamConn = Connections.get<IndexByHSteamNetConnection>();
	ASSERT(bySteamConn.contains(pInfo->m_hConn), "Invalid connection?");

	const auto it = bySteamConn.find(pInfo->m_hConn);

	int identityByteStreamSize = 0;
	const uint8_t *identityByteStream =
		(const uint8_t *)pInfo->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);

	ASSERT(identityByteStream, "Connection with no identity?");
	if (identityByteStream)
	{
		NetworkIdentity realIdentity;

		auto sp = std::span(identityByteStream, identityByteStreamSize);
		ByteReader br(sp);

		realIdentity.Deserialize(br);
		bySteamConn.modify(it,
						   [realIdentity = realIdentity](Connection &c)
						   {
							   c.SetNewState(ConnectionState::eConnected);
							   c.target = realIdentity;
						   });
		ManagingProxy = realIdentity;
	}
}
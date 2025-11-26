#include "Interlink/Interlink.hpp"

#include "Interlink.hpp"
#include "Database/ServerRegistry.hpp"
#include "Database/ProxyRegistry.hpp"
#include "Docker/DockerIO.hpp"
#include <atomic>
#include <chrono>
#include <thread>

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
  case k_ESteamNetworkingConnectionState_None:
    break;
  default:
    logger->ErrorFormatted(std::format("Unknown {}", (int64)pInfo->m_info.m_eState));
    break;
  }
}

void Interlink::SendMessage(const InterlinkMessage &message)
{
  // (unchanged – keep your existing routing if you later use InterlinkMessage)
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
  logger->DebugFormatted("On Connecting");

  auto &IndiciesBySteamConnection = Connections.get<IndexByHSteamNetConnection>();
  auto ExistingConnection = IndiciesBySteamConnection.find(info->m_hConn);

  if (ExistingConnection != IndiciesBySteamConnection.end())
  {
    IndiciesBySteamConnection.modify(
        ExistingConnection, [](Connection &c)
        { c.SetNewState(ConnectionState::eConnecting); });
    logger->DebugFormatted("Existing connection to ({}) started by me transitioning to CONNECTING state", ExistingConnection->target.ToString());
    return;
  }

  /* ADD CHECK TO CHECK IF THE GIVEN NETWORKING IDENTITY ALREADY EXISTS IN THE CONNECTIONS MAP.
  IMPLYING THAT THE SAME TARGET TRIED TO CONNECT MULTIPLE TIMES IN A ROW. CAUSES A CRASH*/

  // ----- New incoming connection path (could be internal or external) -----
  IPAddress address(info->m_info.m_addrRemote);

  int identityByteStreamSize = 0;
  const std::byte *identityByteStream =
      (const std::byte *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);

  // If no identity or malformed, treat as EXTERNAL client (Unity, tools)
  std::optional<InterLinkIdentifier> maybeID;
  if (identityByteStream && info->m_info.m_identityRemote.m_eType == k_ESteamNetworkingIdentityType_GenericBytes)
  {
    maybeID = InterLinkIdentifier::FromEncodedByteStream(identityByteStream, identityByteStreamSize);
  }

  bool known = false;
  InterLinkIdentifier resolvedID;
  if (maybeID.has_value())
  {
    resolvedID = maybeID.value();
    known = ServerRegistry::Get().ExistsInRegistry(resolvedID);
  }
  else
  {
    // Assign a neutral ID for external clients (type GameClient with empty/id-from-ip)
    resolvedID = InterLinkIdentifier::MakeIDGameClient(address.ToString());
  }

  logger->DebugFormatted("Incoming Connection from: {} at {} (known? {})",
                         resolvedID.ToString(), address.ToString(), known ? "yes" : "no");

  // Accept the connection
  if (EResult result = networkInterface->AcceptConnection(info->m_hConn);
      result != k_EResultOK)
  {
    logger->ErrorFormatted("Error accepting connection: reason: {}", uint64(result));
    networkInterface->CloseConnection(info->m_hConn, 0, nullptr, false);
    return;
  }

  // Register new connection (mark external if not registry-known)
  Connection newCon;
  newCon.SteamConnection = info->m_hConn;
  newCon.SetNewState(ConnectionState::eConnecting);
  newCon.address = address;
  newCon.target = resolvedID;
  newCon.kind = known ? ConnectionKind::eInternal : ConnectionKind::eExternal;
  Connections.insert(newCon);
}

void Interlink::CallbackOnClosedByPear(SteamCBInfo info)
{
    auto& bySteam = Connections.get<IndexByHSteamNetConnection>();
    auto it = bySteam.find(info->m_hConn);

    if (it == bySteam.end())
    {
        logger->Warning("ClosedByPeer received for unknown connection.");
        return;
    }

    InterLinkIdentifier closedID = it->target;

    logger->DebugFormatted("Connection closed by peer: {}", closedID.ToString());

    // Remove from internal table BEFORE notifying callbacks.
    bySteam.erase(it);

    // Notify game systems (GameCoordinator, Demigod, etc.)
    DispatchDisconnected(closedID);
}

void Interlink::CallbackOnConnected(SteamCBInfo info)
{
  logger->Debug("OnConnected");
  auto &indiciesBySteamConn = Connections.get<IndexByHSteamNetConnection>();
  if (auto v = indiciesBySteamConn.find(info->m_hConn); v != indiciesBySteamConn.end())
  {
    std::vector<std::pair<InterlinkMessageSendFlag, std::vector<std::byte>>> MessagesToSend;
    indiciesBySteamConn.modify(v, [&MessagesToSend = MessagesToSend](Connection &c)
                               { c.SetNewState(ConnectionState::eConnected);
                                     MessagesToSend = std::move(c.MessagesToSendOnConnect); });

    bool result = networkInterface->SetConnectionPollGroup(v->SteamConnection, PollGroup.value());
    if (!result)
    {
      logger->ErrorFormatted("Failed to assign connection from {} to pollgroup", v->target.ToString());
    }

    if (v->IsExternal())
      logger->DebugFormatted(" - External client Connected from {}", v->address.ToString());
    else
      logger->DebugFormatted(" - {} Connected", v->target.ToString());

    callbacks.OnConnectedCallback(v->target);
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
    IPAddress address(info->m_info.m_addrRemote);
    logger->ErrorFormatted("FUcking idiot. connection not stored internally. {}", address.ToString(true));
    int identityByteStreamSize = 0;
    const std::byte *identityByteStream =
        (const std::byte *)info->m_info.m_identityRemote.GetGenericBytes(identityByteStreamSize);
    if (identityByteStream)
    {
      logger->ErrorFormatted("fucking idoit #2. Identity of remote {}", InterLinkIdentifier::FromEncodedByteStream(identityByteStream, identityByteStreamSize)->ToString());
    }
    ASSERT(false, "Connected? to non existent connection?, HSteamNetConnection was not found");
  }
}

void Interlink::DispatchDisconnected(const InterLinkIdentifier& id)
{
    if (callbacks.OnDisconnectedCallback)
    {
        callbacks.OnDisconnectedCallback(id);
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

  ListeningSocket = networkInterface->CreateListenSocketIP(addr, 1, &opt);
  if (ListeningSocket.value() == k_HSteamListenSocket_Invalid)
  {
    logger->ErrorFormatted("Failed to listen on port {}", port);
    throw std::runtime_error(std::format("Failed to listen on port {}", port));
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

    const Connection &sender = *Connections.get<IndexByHSteamNetConnection>().find(msg->m_conn);

    const void *data = msg->m_pData;
    size_t size = msg->m_cbSize;

    // NEW: God can proxy external messages to internal targets.
    if (MyIdentity.Type == InterlinkType::eGod && sender.IsExternal())
    {
      // Temporary rule: forward to the main GameServer
      // InterLinkIdentifier target = InterLinkIdentifier::MakeIDGameServer("main");
      // logger->DebugFormatted("[Proxy] Forwarding external {} bytes to {}", size, target.ToString());

      // SendMessageRaw(target,
      //                std::span<const std::byte>((const std::byte *)data, size),
      //                InterlinkMessageSendFlag::eReliableNow);
    }
    // Normal internal dispatch
    callbacks.OnMessageArrival(sender, std::span<const std::byte>((const std::byte *)data, size));

    std::string text(reinterpret_cast<const char *>(data), size);
    logger->DebugFormatted("Message from ({}{}) \"{}\"",
                           sender.IsExternal() ? "External " : "",
                           sender.IsExternal() ? sender.address.ToString() : sender.target.ToString(),
                           text);

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

  // Single init per process (no repeated warnings)
  if (!EnsureGNSInitialized())
    return;

  SteamNetworkingUtils()->SetDebugOutputFunction(
      k_ESteamNetworkingSocketsDebugOutputType_Warning,
      [](ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
      {
        Interlink::Get().OnDebugOutput(eType, pszMsg);
      });

  networkInterface = SteamNetworkingSockets();

  // Identity setup (unchanged)
  const auto IdentityByteStream = MyIdentity.ToEncodedByteStream();
  logger->Debug("Settings Networking Identity");
  SteamNetworkingIdentity identity;
  const bool SetIdentity = identity.SetGenericBytes(IdentityByteStream.data(), IdentityByteStream.size());
  ASSERT(SetIdentity, "Failed Identity set");
  networkInterface->ResetIdentity(&identity);
  // Create poll group
  PollGroup = networkInterface->CreatePollGroup();

  IPAddress ipAddress;
  switch (MyIdentity.Type)
  {
  case InterlinkType::eDemigod:
    // Register Demigod in ProxyRegistry
    ListenPort = Type2ListenPort.at(MyIdentity.Type);
    ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":" + std::to_string(ListenPort));
    ProxyRegistry::Get().RegisterSelf(MyIdentity, ipAddress);
    ServerRegistry::Get().RegisterSelf(MyIdentity, ipAddress);
    OpenListenSocket(ListenPort);
    logger->DebugFormatted("[Interlink]Registered in ProxyRegistry as {}:{}", MyIdentity.ToString(), ipAddress.ToString());

    break;
  case InterlinkType::eGameClient:

    break;
  default:
    // Register internal (container) address
    IPAddress ipAddress;
    ListenPort = Type2ListenPort.at(MyIdentity.Type);
    ipAddress.Parse(DockerIO::Get().GetSelfContainerIP() + ":" + std::to_string(ListenPort));
    ServerRegistry::Get().RegisterSelf(MyIdentity, ipAddress);
    logger->DebugFormatted("[Interlink]Registered in ServerRegistry as {}:{}", MyIdentity.ToString(), ipAddress.ToString());
    // Open internal mesh listener
    OpenListenSocket(ListenPort);
    break;
  }

  logger->Debug("Registered local");

  // Register public address (host ip:published port), if available
  if (DockerIO::Get().GetSelfExposedPorts().empty() == false)
  {
    // The port we are actually listening on inside the container
    ListenPort = Type2ListenPort.at(MyIdentity.Type);

    // Find the HOST (public) port that Docker mapped for THIS internal listen port
    if (auto mappedHostPort = DockerIO::Get().GetSelfExposedPortForInternalBind(ListenPort))
    {
      // Get the host's public (or LAN) IP; ignore the outPort here
      uint32_t ignore = 0;
      const std::string hostIP = DockerIO::Get().GetSelfPublicIP(ignore);

      IPAddress pubAddr;
      pubAddr.Parse(hostIP + ":" + std::to_string(*mappedHostPort));

      ServerRegistry::Get().RegisterPublicAddress(MyIdentity, pubAddr);
      logger->DebugFormatted("[Interlink] Registered public address: {}", pubAddr.ToString());
    }
    /*
              if (auto mappedHostPort = DockerIO::Get().GetSelfExposedPortForInternalBind(ListenPort))
      {
          SteamNetworkingIPAddr publicAddr;
          uint32_t ignored = 0;
          std::string hostIP = DockerIO::Get().GetSelfPublicIP(ignored);
          publicAddr.ParseString(hostIP.c_str());
          publicAddr.m_port = *mappedHostPort;

          SteamNetworkingConfigValue_t opt;
          opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                    (void *)SteamNetConnectionStatusChanged);

          auto PublicSocket = networkInterface->CreateListenSocketIP(publicAddr, 1, &opt);
          if (PublicSocket == k_HSteamListenSocket_Invalid)
              logger->Error("Failed to open public listen socket on");
          else
              logger->Debug("Opened public listen socket on");
      }
    */
  }
  else
  {
    logger->DebugFormatted("[Interlink] No host port published for internal {} ({}). Skipping public registration.",
                           ListenPort, MyIdentity.ToString());
  }

  logger->Debug("Registered public");

  // Existing post-init behavior (unchanged)
  switch (MyIdentity.Type)
  {
  case InterlinkType::eGameServer:
  {
    InterLinkIdentifier PartitionID = MyIdentity;
    PartitionID.Type = InterlinkType::ePartition;
    EstablishConnectionTo(PartitionID);
  }
  break;

  case InterlinkType::eGameClient:
  {
    // InterLinkIdentifier targetServer(MyIdentity);
    // targetServer.Type = InterlinkType::eGameServer;
    // EstablishConnectionTo(targetServer);
  }
  break;

  case InterlinkType::ePartition:
  {
    EstablishConnectionTo(InterLinkIdentifier::MakeIDGod());
  }
  break;

  case InterlinkType::eDemigod:
  {
    // connect with everything for now to test
    std::unordered_map<InterLinkIdentifier, ServerRegistryEntry> serverMap =
        ServerRegistry::Get().GetServers();

    for (auto &server : serverMap)
    {
      if (server.first.Type == InterlinkType::ePartition ||
          server.first.Type == InterlinkType::eGod ||
          server.first.Type == InterlinkType::eGameCoordinator)
      {
        EstablishConnectionTo(server.first);
      }
    }
  }
  break;

  case InterlinkType::eGameCoordinator:
  {
    // std::unordered_map<InterLinkIdentifier, ProxyRegistry::ProxyRegistryEntry> proxyMap =
    //     ProxyRegistry::Get().GetProxies();
    //
    // for (auto &proxy : proxyMap)
    //{
    //  EstablishConnectionTo(proxy.first);
    //}
  }
  break;

  case InterlinkType::eGod:
  default:
    break;
  }
}

void Interlink::Shutdown()
{
  logger->Debug("Interlink Shutdown");
}

bool Interlink::EstablishConnectionAtIP(const InterLinkIdentifier &id, const IPAddress &address)
{
  // return EstablishConnectionTo(InterLinkIdentifier::MakeIDGod());
  if (Connections.get<IndexByTarget>().contains(id))
  {
    const Connection &existing = *Connections.get<IndexByTarget>().find(id);
    if (existing.state == ConnectionState::eConnected)
    {
      logger->WarningFormatted("Connection to {} already established", id.ToString());
      return true;
    }
    else if (existing.state == ConnectionState::eConnecting || existing.state == ConnectionState::ePreConnecting)
    {
      logger->WarningFormatted("Already connecting to {}", id.ToString());
      return true;
    }
  }

  // Step 1: Build God ID
  InterLinkIdentifier godID = InterLinkIdentifier::MakeIDGod();

  // Step 2: Try to get public IP:port from registry
  // std::optional<IPAddress> publicAddr = ServerRegistry::Get().GetPublicAddress(godID);
  // if (!publicAddr.has_value())
  //{
  //  logger->ErrorFormatted("No public address found for {} in Server Registry", godID.ToString());
  //  return false;
  //}
  // else
  //{
  //  std::cout << "God is reachable at " << publicAddr->ToString() << std::endl;
  //}

  // auto IP = ServerRegistry::Get().GetIPOfID(id);

  Connection conn;
  conn.address = address;
  conn.target = id;
  // conn.address = *publicAddr;
  // conn.target = godID;
  conn.SetNewState(ConnectionState::ePreConnecting);
  conn.kind = ConnectionKind::eExternal; // external connection path (direct IP)
  Connections.insert(conn);

  logger->DebugFormatted("Establishing direct connection to {} at {}", id.ToString(), address.ToString());
  return true;
}

bool Interlink::EstablishConnectionTo(const InterLinkIdentifier &id)
{
  ASSERT(MyIdentity.Type != InterlinkType::eGameClient, "Game client must use the ip one");
  // Prevent duplicate attempts
  if (Connections.get<IndexByTarget>().contains(id))
  {
    const Connection &existing = *Connections.get<IndexByTarget>().find(id);
    if (existing.state == ConnectionState::eConnected)
    {
      logger->WarningFormatted("Connection to {} already established", id.ToString());
      return true;
    }
    if (existing.state == ConnectionState::eConnecting || existing.state == ConnectionState::ePreConnecting)
    {
      logger->WarningFormatted("Already connecting to {}", id.ToString());
      return true;
    }
  }
  // ---------------------------------------------------------------
  // INTERNAL: must exist in ServerRegistry
  // ---------------------------------------------------------------
  if (id.Type == InterlinkType::eGod ||
      id.Type == InterlinkType::ePartition ||
      id.Type == InterlinkType::eGameServer ||
      id.Type == InterlinkType::eGameCoordinator)
  {
    auto IP = ServerRegistry::Get().GetIPOfID(id);

    for (int i = 0; i < 5 && !IP.has_value(); i++)
    {
      logger->ErrorFormatted("IP not found for {} in Server Registry. Trying again in 1 second", id.ToString());
      std::this_thread::sleep_for(std::chrono::seconds(1));
      IP = ServerRegistry::Get().GetIPOfID(id);
    }

    if (!IP.has_value())
    {
      logger->ErrorFormatted("Failed to establish connection after 5 tries. IP not found in Server Registry {}", id.ToString());
      return false; // no assert crash, just graceful fail
    }

    Connection conn;
    conn.address = IP.value();
    conn.target = id;
    conn.kind = ConnectionKind::eInternal;
    conn.SetNewState(ConnectionState::ePreConnecting);
    Connections.insert(conn);

    logger->DebugFormatted("Establishing internal connection to {}", id.ToString());
    return true;
  }
  // ---------------------------------------------------------------
  // INTERNAL: must exist in ProxyRegistry
  // ---------------------------------------------------------------
  if (id.Type == InterlinkType::eDemigod)
  {
    auto IP = ProxyRegistry::Get().GetIPOfID(id);

    for (int i = 0; i < 5 && !IP.has_value(); i++)
    {
      logger->ErrorFormatted("IP not found for {} in ProxyRegistry Registry. Trying again in 1 second", id.ToString());
      std::this_thread::sleep_for(std::chrono::seconds(1));
      IP = ProxyRegistry::Get().GetIPOfID(id);
    }

    if (!IP.has_value())
    {
      logger->ErrorFormatted("Failed to establish connection after 5 tries. IP not found in ProxyRegistry Registry {}", id.ToString());
      return false; // no assert crash, just graceful fail
    }

    Connection conn;
    conn.address = IP.value();
    conn.target = id;
    conn.kind = ConnectionKind::eInternal;
    conn.SetNewState(ConnectionState::ePreConnecting);
    Connections.insert(conn);

    logger->DebugFormatted("Establishing internal connection to {}", id.ToString());
    return true;
  }

  // ---------------------------------------------------------------
  // EXTERNAL: skip registry (for now, nothing directly connects to clients)
  // ---------------------------------------------------------------
  if (id.Type == InterlinkType::eGameClient)
  {
    // For now, external clients are expected to connect INBOUND to God.
    // Outbound from inside to a GameClient is not required.
    logger->WarningFormatted("Skipping registry lookup for external client {}", MyIdentity.ToString());
    return true;
  }

  // Default case (unknown type)
  logger->WarningFormatted("Unknown interlink type for {} - skipping connection", MyIdentity.ToString());
  return false;
}

void Interlink::CloseConnectionTo(const InterLinkIdentifier& id, int reason, const char* debug)
{
    auto& byTarget = Connections.get<IndexByTarget>();

    auto it = byTarget.find(id);
    if (it == byTarget.end())
    {
        logger->WarningFormatted("CloseConnectionTo: No active connection found for {}", id.ToString());
        return;
    }

    HSteamNetConnection conn = it->SteamConnection;

    logger->DebugFormatted("Closing connection to {} (SteamConn={})", id.ToString(), (uint64)conn);

    // Close on the networking side
    networkInterface->CloseConnection(conn, reason, debug, false);

    // Remove from table
    byTarget.erase(it);

    DispatchDisconnected(id);
}


void Interlink::DebugPrint()
{
  for (const auto &connection : Connections)
  {
    logger->Debug(connection.ToString());
  }
}

void Interlink::SendMessageRaw(const InterLinkIdentifier &who, std::span<const std::byte> data, InterlinkMessageSendFlag sendFlag)
{
  auto QueueMessageOnConnect = [&]()
  {
    auto it = Connections.get<IndexByTarget>().find(who);
    Connections.get<IndexByTarget>().modify(it, [&](Connection &c)
                                            {
            std::vector<std::byte> newdata;
            newdata.insert(newdata.end(), data.begin(), data.end());
            c.MessagesToSendOnConnect.push_back(std::make_pair(sendFlag,newdata)); });
  };

  if (!Connections.get<IndexByTarget>().contains(who))
  {
    logger->DebugFormatted("Connection to \"{}\" was not established, connecting...", who.ToString());
    EstablishConnectionTo(who);
    QueueMessageOnConnect();
  }
  else if (auto find = Connections.get<IndexByTarget>().find(who); find->state != ConnectionState::eConnected)
  {
    if (find->state == ConnectionState::ePreConnecting || find->state == ConnectionState::eConnecting)
    {
      logger->DebugFormatted("Connection to {} is {} , queuing message...",
                             who.ToString(),
                             boost::describe::enum_to_string(Connections.get<IndexByTarget>().find(who)->state, "unknown"));
      QueueMessageOnConnect();
    }
    else
    {
      logger->DebugFormatted("Connection to {} state is {} , connecting...",
                             who.ToString(),
                             boost::describe::enum_to_string(Connections.get<IndexByTarget>().find(who)->state, "unknown"));
      EstablishConnectionTo(who);
      QueueMessageOnConnect();
    }
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
}

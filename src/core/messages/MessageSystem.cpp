#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/job/JobContext.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobOptions.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "enviroment/Enviroment.hpp"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"

#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <vector>

AtlasNet::MessageSystem::MessageSystem(const Config& config) : config_(config)
{
  AN_ASSERT(config_.jobSystem, "JobSystem must be provided");
  SteamNetworkingErrMsg errMsg;
  const bool result = GameNetworkingSockets_Init(nullptr, errMsg);
  if (!result)
  {
    throw std::runtime_error(
        std::string("Failed to initialize GameNetworkingSockets: ") + errMsg);
  }

  SteamNetworkingUtils()->SetDebugOutputFunction(
      k_ESteamNetworkingSocketsDebugOutputType_Debug,
      [](ESteamNetworkingSocketsDebugOutputType, const char* pszMsg)
      { std::cerr << "GNS Debug: " << pszMsg << std::endl; });

  _GNS = SteamNetworkingSockets();

  _pollGroup = GNS().CreatePollGroup();
  if (_pollGroup == k_HSteamNetPollGroup_Invalid)
  {
    GameNetworkingSockets_Kill();
    throw std::runtime_error("Failed to create SteamNetworking poll group");
  }
  _pollJobHandle = config_.jobSystem->Submit(
      [this](JobContext& handle)
      {
        if (shutdown.load(std::memory_order_acquire))
        {
          std::cerr << "MessageSystem poll job exiting due to shutdown signal."
                    << std::endl;
          return;
        }
        else
        {
          handle.repeat_once(
              std::chrono::milliseconds(1000 / EnvVars::TickRate));
        }

        GNS().RunCallbacks();
        _Parse_Incoming_Messages();
      },

      JobOpts::Name("MessageSystem::Poll"),
      JobOpts::Notify<JobNotifyLevel::eNone>(),
      JobOpts::TPriority<JobPriority::eHigh>{});
}

AtlasNet::MessageSystem::ListenSocketHandle&
AtlasNet::MessageSystem::OpenListenSocket(PortType port)
{
  std::unique_lock lock(_mutex);

  if (_listenSockets.contains(port))
  {
    throw std::runtime_error("Listen socket already open");
  }

  SteamNetworkingIPAddr localAddr;
  localAddr.Clear();
  localAddr.m_port = port;

  SteamNetworkingConfigValue_t opts[2];
  opts[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                 (void*)&OnSteamNetConnectionStatusChanged);
  opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, (int64_t)this);

  const HSteamListenSocket listenSocket =
      GNS().CreateListenSocketIP(localAddr, 2, opts);

  if (listenSocket == k_HSteamListenSocket_Invalid)
  {
    throw std::runtime_error("Failed to create listen socket on port " +
                             std::to_string(port));
  }

  std::cerr << "Opened listen socket on port " << port << std::endl;

  auto inserted = _listenSockets.emplace(
      port, std::make_unique<ListenSocketHandle>(*this, listenSocket, port));

  return *inserted.first->second;
}

AtlasNet::MessageSystem::ListenSocketHandle&
AtlasNet::MessageSystem::GetListenSocket(PortType port)
{
  std::shared_lock lock(_mutex);

  auto it = _listenSockets.find(port);
  if (it == _listenSockets.end())
  {
    throw std::runtime_error("Listen socket not open");
  }

  return *it->second;
}

void AtlasNet::MessageSystem::SteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  if (!pInfo)
    return;

  SocketAddress address(pInfo->m_info.m_addrRemote);

  switch (pInfo->m_info.m_eState)
  {
  case k_ESteamNetworkingConnectionState_Connecting:
  {
    const bool isIncoming =
        (pInfo->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);

    if (isIncoming)
    {
      GNS().SetConnectionUserData(pInfo->m_hConn, (int64)this);
      const EResult r = GNS().AcceptConnection(pInfo->m_hConn);
      if (r != k_EResultOK)
      {
        std::cout << "AcceptConnection failed: " << static_cast<int>(r)
                  << " for " << pInfo->m_info.m_szConnectionDescription
                  << std::endl;

        GNS().CloseConnection(pInfo->m_hConn, 0, "AcceptConnection failed",
                              false);
        break;
      }

      if (!GNS().SetConnectionPollGroup(pInfo->m_hConn, _pollGroup))
      {
        std::cerr << "Failed to assign accepted connection to poll group"
                  << std::endl;
        GNS().CloseConnection(pInfo->m_hConn, 0, "Failed to assign poll group",
                              false);
        break;
      }

      {
        std::unique_lock lock(_mutex);

        Connection conn(*this, pInfo->m_hConn);
        conn.state = ConnectionState::eConnecting;

        auto it = _connections.find(address);
        if (it == _connections.end())
        {
          _connections.emplace(address, std::move(conn));
        }
        else
        {
          it->second.state = ConnectionState::eConnecting;
        }
      }

      std::cout << "Accepted incoming connection: "
                << pInfo->m_info.m_szConnectionDescription << std::endl;
    }
    else
    {
      std::cout << "Outgoing connection (initiated locally): "
                << pInfo->m_info.m_szConnectionDescription << std::endl;
    }

    break;
  }

  case k_ESteamNetworkingConnectionState_Connected:
  {
    std::unique_lock lock(_mutex);
    auto it = _connections.find(address);
    if (it != _connections.end())
    {
      it->second.state = ConnectionState::eConnected;
    }
    else
    {
      // Defensive: callback may win race against insertion path.
      Connection conn(*this, pInfo->m_hConn);
      conn.state = ConnectionState::eConnected;
      _connections.emplace(address, std::move(conn));
    }
  }

    std::cout << "Connection connected: "
              << pInfo->m_info.m_szConnectionDescription << std::endl;
    break;

  case k_ESteamNetworkingConnectionState_ClosedByPeer:
  {
    std::unique_lock lock(_mutex);
    auto it = _connections.find(address);
    if (it != _connections.end())
    {
      it->second.state = ConnectionState::eClosedByPeer;
    }
  }

    std::cout << "Connection closed by peer: "
              << pInfo->m_info.m_szConnectionDescription << std::endl;
    break;

  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
  {
    std::unique_lock lock(_mutex);
    auto it = _connections.find(address);
    if (it != _connections.end())
    {
      it->second.state = ConnectionState::eProblemDetectedLocally;
    }
  }

    std::cout << "Problem detected locally: "
              << pInfo->m_info.m_szConnectionDescription << std::endl;
    break;

  case k_ESteamNetworkingConnectionState_None:
  case k_ESteamNetworkingConnectionState_FindingRoute:
  case k_ESteamNetworkingConnectionState_FinWait:
  case k_ESteamNetworkingConnectionState_Linger:
  case k_ESteamNetworkingConnectionState_Dead:
  case k_ESteamNetworkingConnectionState__Force32Bit:
    break;
  }
}

AtlasNet::ConnectionState AtlasNet::MessageSystem::GetConnectionState(
    const SocketAddress& address) const
{
  std::shared_lock lock(_mutex);

  auto connectionIt = _connections.find(address);
  if (connectionIt != _connections.end())
  {
    const HSteamNetConnection handle = connectionIt->second.GetHandle();
    SteamNetConnectionInfo_t info;
    if (handle != k_HSteamNetConnection_Invalid &&
        GNS().GetConnectionInfo(handle, &info))
    {
      return static_cast<ConnectionState>(info.m_eState);
    }
  }

  return ConnectionState::eNone;
}

bool AtlasNet::MessageSystem::IsConnectingTo(
    const SocketAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnecting;
}

bool AtlasNet::MessageSystem::IsConnectedTo(
    const SocketAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnected;
}

AtlasNet::JobHandle
AtlasNet::MessageSystem::Connect(const SocketAddress& address)
{

  {
    std::shared_lock lock(_mutex);

    // Reuse an existing connect job if one is already tracked.
    auto existingJobIt = _connectJobs.find(address);
    if (existingJobIt != _connectJobs.end())
    {
      return existingJobIt->second;
    }

    // If already connected, return a completed no-op job so callers can
    // still safely chain on_complete() if they want.
    auto connIt = _connections.find(address);
    if (connIt != _connections.end() &&
        connIt->second.GetState() == ConnectionState::eConnected)
    {
      return config_.jobSystem->Submit(
          [](JobContext&) {},
          JobOpts::Name(std::format("MessageSystem::AlreadyConnected {}",
                                    address.to_string())),
          JobOpts::Notify<JobNotifyLevel::eNone>(),
          JobOpts::TPriority<JobPriority::eHigh>{});
    }
  }

  // Submit outside any long-lived write logic? Normally yes.
  // But to avoid duplicate jobs for the same address, we still need to
  // serialize the "check + insert" with a unique lock before returning.
  std::unique_lock lock(_mutex);

  // Double-check after upgrading.
  auto existingJobIt = _connectJobs.find(address);
  if (existingJobIt != _connectJobs.end())
  {
    return existingJobIt->second;
  }

  auto connIt = _connections.find(address);
  if (connIt != _connections.end() &&
      connIt->second.GetState() == ConnectionState::eConnected)
  {
    lock.unlock();
    return config_.jobSystem->Submit(
        [](JobContext&) {},
        JobOpts::Name(std::format("MessageSystem::AlreadyConnected {}",
                                  address.to_string())),
        JobOpts::Notify<JobNotifyLevel::eNone>(),
        JobOpts::TPriority<JobPriority::eHigh>{});
  }

  JobHandle handle = config_.jobSystem->Submit(
      [this, address](JobContext& ctx)
      {
        _Connect_to_job(ctx, address);

        // Clean up tracked connect jobs once the connection reaches
        // a terminal or ready state.
        const ConnectionState state = GetConnectionState(address);
        if (state == ConnectionState::eConnected ||
            state == ConnectionState::eClosedByPeer ||
            state == ConnectionState::eProblemDetectedLocally ||
            state == ConnectionState::eNone)
        {
          std::unique_lock cleanupLock(_mutex);
          _connectJobs.erase(address);
        }
      },
      JobOpts::TPriority<JobPriority::eHigh>{},
      JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{},
      JobOpts::Name(
          std::format("MessageSystem::ConnectTo {}", address.to_string())));

  _connectJobs[address] = handle;
  return handle;
}

void AtlasNet::MessageSystem::Shutdown()
{
  if (shutdown.exchange(true, std::memory_order_acq_rel))
  {
    return;
  }
  _pollJobHandle->wait();
  std::cerr << "Shutting down MessageSystem..." << std::endl;

  // If your JobSystem supports cancellation, do it here.
  // Example:
  // _pollJobHandle.Cancel();
  //
  // And also cancel/wait any connect jobs in _connectJobs.

  std::vector<HSteamNetConnection> connectionHandles;
  std::vector<HSteamListenSocket> listenHandles;

  {
    std::unique_lock lock(_mutex);

    connectionHandles.reserve(_connections.size());
    for (const auto& [address, connection] : _connections)
    {
      (void)address;
      connectionHandles.push_back(connection.GetHandle());
    }
    _connections.clear();

    listenHandles.reserve(_listenSockets.size());
    for (const auto& [port, listenSocket] : _listenSockets)
    {
      (void)port;
      if (listenSocket)
      {
        listenHandles.push_back(listenSocket->handle);
      }
    }
    _listenSockets.clear();

    _connectJobs.clear();
  }

  for (HSteamNetConnection handle : connectionHandles)
  {
    if (handle != k_HSteamNetConnection_Invalid)
    {
      GNS().CloseConnection(handle, 0, "Shutting down", false);
    }
  }

  for (HSteamListenSocket handle : listenHandles)
  {
    if (handle != k_HSteamListenSocket_Invalid)
    {
      GNS().CloseListenSocket(handle);
    }
  }

  if (_pollGroup != k_HSteamNetPollGroup_Invalid)
  {
    GNS().DestroyPollGroup(_pollGroup);
    _pollGroup = k_HSteamNetPollGroup_Invalid;
  }

  GameNetworkingSockets_Kill();
  std::cerr << "MessageSystem shutdown complete." << std::endl;
}

AtlasNet::MessageSystem::~MessageSystem()
{
  Shutdown();
}

void AtlasNet::MessageSystem::SetIdentity(
    const SteamNetworkingIdentity& identity)
{
  GNS().ResetIdentity(&identity);
}

void AtlasNet::MessageSystem::ListenSocketHandle::DispatchCallbacks(
    const IMessage& message, MessageIDHash typeIdHash,
    const SocketAddress& caller_address)
{
  HandlerFunc dispatcher;
  bool found = false;

  {
    std::shared_lock lock(socket_mutex);
    auto it = _handlers.find(typeIdHash);
    if (it != _handlers.end())
    {
      dispatcher = it->second;
      found = true;
    }
  }

  if (found)
  {
    std::cerr << std::format(
        "Dispatching message of type hash {} received on listen socket port {} to socket dispatcher\n",
        typeIdHash, port)
              << std::endl;
    dispatcher(message, caller_address);
  }
  else
  {
    std::cerr << std::format(
        "No dispatcher registered for message type with hash {}\n", typeIdHash);
  }
}
void AtlasNet::MessageSystem::MessageSystem::_Parse_Incoming_Messages()
{

  ISteamNetworkingMessage* pIncomingMessages[32] = {};
  const int numMsgs = GNS().ReceiveMessagesOnPollGroup(
      _pollGroup, pIncomingMessages,
      static_cast<int>(std::size(pIncomingMessages)));

  if (numMsgs < 0)
  {
    std::cerr << "ReceiveMessagesOnPollGroup failed" << std::endl;
    return;
  }

  for (int i = 0; i < numMsgs; ++i)
  {
    ISteamNetworkingMessage* msg = pIncomingMessages[i];
    if (!msg)
      continue;

    SteamNetConnectionInfo_t info;
    if (!GNS().GetConnectionInfo(msg->m_conn, &info))
    {
      std::cerr << "Failed to get connection info for incoming message"
                << std::endl;
      msg->Release();
      continue;
    }

    std::optional<PortType> ListenSocketPortReceivedOn = std::nullopt;
    if (info.m_hListenSocket != k_HSteamListenSocket_Invalid)
    {
      SteamNetworkingIPAddr listenAddr;
      if (GNS().GetListenSocketAddress(info.m_hListenSocket, &listenAddr))
      {
        ListenSocketPortReceivedOn = listenAddr.m_port;
      }
    }

    std::optional<SocketAddress> addressRemote;
    if (info.m_addrRemote.IsIPv4())
    {
      const uint32 ip4Packed = info.m_addrRemote.GetIPv4();
      const uint16 port = info.m_addrRemote.m_port;
      addressRemote = SocketAddress(IPv4(ip4Packed), port);
    }
    else
    {
      addressRemote = SocketAddress(IPv6(info.m_addrRemote.m_ipv6),
                                      info.m_addrRemote.m_port);
    }
    std::cout << std::format("Incoming message from {}",
                             addressRemote->to_string())
              << std::endl;

    AN_ASSERT(addressRemote->IsValid(),
              "Invalid remote address in incoming message");

    ByteReader readerID(std::span(static_cast<const uint8_t*>(msg->m_pData),
                                  static_cast<std::size_t>(msg->m_cbSize)));

    const MessageIDHash typeIdHash = IMessage::DeserializeTypeIdHash(readerID);
    std::cout << std::format("Message of type hash: {} {}", typeIdHash,
                             ListenSocketPortReceivedOn.has_value()
                                 ? std::format("on listen socket port {}",
                                               *ListenSocketPortReceivedOn)
                                 : "on client port")
              << std::endl;

    // Copy dispatcher out while holding the lock, then invoke
    // unlocked.
    DispatchFunc dispatcher;
    bool found = false;
    {
      std::shared_lock lock(_mutex);
      auto it = _dispatchTable.find(typeIdHash);
      if (it != _dispatchTable.end())
      {
        dispatcher = it->second;
        found = true;
      }
    }

    if (found)
    {
      std::vector<uint8_t> bytes;
      bytes.assign(static_cast<const uint8_t*>(msg->m_pData),
                   static_cast<const uint8_t*>(msg->m_pData) +
                       static_cast<std::size_t>(msg->m_cbSize));

      config_.jobSystem->Submit(
          [this, dispatcher = dispatcher, bytes = std::move(bytes),
           addressRemote = *addressRemote,
           port_received_on = ListenSocketPortReceivedOn](JobContext&)
          {
            ByteReader readerFull(bytes);
            AN_ASSERT(dispatcher != nullptr,
                      "Dispatcher should not be null here");
            dispatcher(readerFull, addressRemote, port_received_on);
          },
          JobOpts::Name(
              std::format("MessageSystem::DispatchMessage typeHash {} from {}",
                          typeIdHash, addressRemote->to_string())),
          JobOpts::TPriority<JobPriority::eHigh>{},
          JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
    }

    msg->Release();
  }
}
HSteamNetConnection AtlasNet::MessageSystem::GetConnectionHandle(
    const SocketAddress& address) const
{
  std::shared_lock lock(_mutex);

  if (_connections.contains(address))
  {
    return _connections.at(address).GetHandle();
  }
  std::cerr << "Connection handle requested for non-existent connection to "
            << address.to_string() << std::endl;
  std::cerr << "Known connections:\n";
  for (const auto& [addr, conn] : _connections)
  {
    std::cerr << " - " << addr.to_string() << std::endl;
  }
  throw std::runtime_error("Connection not found");
}
void AtlasNet::MessageSystem::_Connect_to_job(JobContext& handle,
                                              const SocketAddress& address)
{

  if (shutdown.load(std::memory_order_acquire))
  {
    return;
  }

  const ConnectionState state = GetConnectionState(address);

  if (state != ConnectionState::eConnected &&
      state != ConnectionState::eConnecting)
  {
    SteamNetworkingConfigValue_t opts[2];
    opts[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                   (void*)&OnSteamNetConnectionStatusChanged);
    opts[1].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData,
                     (int64_t)this);

    SteamNetworkingIPAddr steamAddr = address.ToSteamAddr();
    const HSteamNetConnection con =
        GNS().ConnectByIPAddress(steamAddr, 2, opts);

    if (con == k_HSteamNetConnection_Invalid)
    {
      char errMsg[1024] = {};
      steamAddr.ToString(errMsg, sizeof(errMsg), true);
      std::cerr << "Failed to connect to " << errMsg << std::endl;
      return;
    }

    if (!GNS().SetConnectionPollGroup(con, _pollGroup))
    {
      GNS().CloseConnection(con, 0, "Failed to assign poll group", false);
      std::cerr << "Failed to assign outgoing connection to poll group"
                << std::endl;
      return;
    }

    {
      std::unique_lock lock(_mutex);
      Connection conn(*this, con);
      conn.state = ConnectionState::eConnecting;

      auto it = _connections.find(address);
      if (it == _connections.end())
      {
        _connections.emplace(address, std::move(conn));
      }
      else
      {
        it->second.state = ConnectionState::eConnecting;
      }
    }

    std::cerr << "Initiated connection to " << address.to_string() << std::endl;

    handle.repeat_once(std::chrono::milliseconds(1000 / EnvVars::TickRate));
    return;
  }

  if (state == ConnectionState::eConnecting)
  {
    handle.repeat_once(std::chrono::milliseconds(1000 / EnvVars::TickRate));
  }
}
std::optional<AtlasNet::MessageSystem::Connection>
AtlasNet::MessageSystem::GetConnection(const SocketAddress& address) const
{
  std::shared_lock lock(_mutex);
  if (_connections.contains(address))
  {
    return _connections.at(address);
  }
  return std::nullopt;
}
void AtlasNet::MessageSystem::GetConnections(
    std::unordered_map<SocketAddress, Connection>& connections) const
{
  std::shared_lock lock(_mutex);
  connections = _connections;
}
size_t AtlasNet::MessageSystem::GetNumConnections() const
{
  std::shared_lock lock(_mutex);
  return _connections.size();
}
AtlasNet::MessageSystem::ListenSocketHandle::ListenSocketHandle(
    MessageSystem& system, HSteamListenSocket handle,PortType port)
    : system(system), handle(handle), port(port)
{
}
void AtlasNet::MessageSystem::Connection::SendMessage(
    const void* data, uint32_t size, MessageSendMode mode) const
{
  system.GNS().SendMessageToConnection(handle, data, size,
                                       static_cast<int>(mode), nullptr);
}

#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/job/Job.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "enviroment/Enviroment.hpp"
#include "steam/isteamnetworkingutils.h"
#include "steam/steamnetworkingtypes.h"
#include <iostream>
#include <mutex>
#include <shared_mutex>

AtlasNet::MessageSystem::MessageSystem()
{
  ContainerID ID = ContainerInfo::Get().GetID();
  SteamNetworkingErrMsg errMsg;
  bool result = GameNetworkingSockets_Init(nullptr, errMsg);
  if (!result)
  {
    throw std::runtime_error(
        std::string("Failed to initialize GameNetworkingSockets: ") + errMsg);
  }
  SteamNetworkingUtils()->SetDebugOutputFunction(
      k_ESteamNetworkingSocketsDebugOutputType_Debug,
      [](ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg)
      { std::cerr << "GNS Debug: " << pszMsg << std::endl; });

  _GNS = SteamNetworkingSockets();

  _pollGroup = GNS().CreatePollGroup();
  if (_pollGroup == k_HSteamNetPollGroup_Invalid)
  {
    throw std::runtime_error("Failed to create SteamNetworking poll group");
  }
  Job pollJob =
      Job([this](Job::JobContext& handle) { GNS().RunCallbacks(); })
          .repeating(std::chrono::milliseconds(1000 / EnvVars::TickRate))
          .set_name("MessageSystem::Poll")
          .set_notify_level(JobNotifyLevel::eNone)
          .set_priority(JobPriority::eHigh);

  JobSystem& jobSystem = JobSystem::Get();

  jobSystem.PushJob(pollJob);
};

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
  SteamNetworkingConfigValue_t opt;
  opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
             (void*)&OnSteamNetConnectionStatusChanged);

  HSteamListenSocket listenSocket =
      GNS().CreateListenSocketIP(localAddr, 1, &opt);
  if (listenSocket == k_HSteamListenSocket_Invalid)
  {
    throw std::runtime_error("Failed to create listen socket on port " +
                             std::to_string(port));
  }
  std::cerr << "Opened listen socket on port " << port << std::endl;
  _listenSockets.emplace(port, ListenSocketHandle(*this, listenSocket));
  return _listenSockets.at(port);
};
AtlasNet::MessageSystem::ListenSocketHandle&
AtlasNet::MessageSystem::GetListenSocket(PortType port)
{
  std::shared_lock lock(_mutex);
  if (!_listenSockets.contains(port))
  {
    throw std::runtime_error("Listen socket not open");
  }
  return _listenSockets.at(port);
}
void AtlasNet::MessageSystem::SteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* pInfo)
{
  EndPointAddress address(pInfo->m_info.m_addrRemote);
  switch (pInfo->m_info.m_eState)
  {
  case k_ESteamNetworkingConnectionState_Connecting:
  {
    const bool isIncoming =
        (pInfo->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid);

    if (isIncoming)
    {
      EResult r = GNS().AcceptConnection(pInfo->m_hConn);
      if (r != k_EResultOK)
      {
        std::cout << "AcceptConnection failed: " << static_cast<int>(r)
                  << " for " << pInfo->m_info.m_szConnectionDescription
                  << std::endl;
        GNS().CloseConnection(pInfo->m_hConn, 0, "AcceptConnection failed",
                              false);
        break;
      }
      std::unique_lock lock(_mutex);
      _connections.emplace(address, Connection(*this, pInfo->m_hConn));

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
    std::cout << "Connection connected: "
              << pInfo->m_info.m_szConnectionDescription << std::endl;
    break;
  case k_ESteamNetworkingConnectionState_ClosedByPeer:
    std::cout << "Connection closed by peer: "
              << pInfo->m_info.m_szConnectionDescription << std::endl;
    break;
  case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
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
    const EndPointAddress& address) const
{
  std::shared_lock lock(_mutex);

  if (auto connectionIt = _connections.find(address);
      connectionIt != _connections.end())
  {
    HSteamNetConnection handle = connectionIt->second.GetHandle();
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
    const EndPointAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnecting;
}
bool AtlasNet::MessageSystem::IsConnectedTo(
    const EndPointAddress& address) const
{
  return GetConnectionState(address) == ConnectionState::eConnected;
}
AtlasNet::JobHandle
AtlasNet::MessageSystem::Connect(const EndPointAddress& address)
{
  Job connectJob =
      Job(
          [this, address](Job::JobContext& handle)
          {
            ConnectionState state = GetConnectionState(address);
            if (state != ConnectionState::eConnected &&
                state != ConnectionState::eConnecting)
            {
              SteamNetworkingConfigValue_t opt;
              opt.SetPtr(
                  k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                  (void*)&OnSteamNetConnectionStatusChanged);
              SteamNetworkingIPAddr steamAddr = address.ToSteamAddr();
              HSteamNetConnection con =
                  GNS().ConnectByIPAddress(steamAddr, 1, &opt);

              if (con == k_HSteamNetConnection_Invalid)
              {
                char errMsg[1024];
                steamAddr.ToString(errMsg, sizeof(errMsg), true);
                
                std::cerr << "Failed to connect to " << errMsg
                          << std::endl;
                return;
              }
              GNS().SetConnectionPollGroup(con, _pollGroup);
              std::cerr << "Initiated connection to " << address.to_string()
                        << std::endl;
              _connections.emplace(address, Connection(*this, con));
            }
            if (state == ConnectionState::eConnecting)
            {
              handle.repeat_once(
                  std::chrono::milliseconds(1000 / EnvVars::TickRate));
            }
          })
          .set_priority(JobPriority::eHigh)
          .set_name(
              std::format("MessageSystem::ConnectTo {}", address.to_string()))
          .set_notify_level(JobNotifyLevel::eOnStart);

  JobHandle handle = JobSystem::Get().PushJob(connectJob);
  _connectJobs.emplace(address, handle);
  return handle;
}

#pragma once
#include "Message.hpp"
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/container/Container.hpp"
#include "atlasnet/core/EndPoint.hpp"
#include "atlasnet/core/job/Job.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "enviroment/Enviroment.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <atomic>
#include <format>
#include <iostream>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
namespace AtlasNet
{
static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info);
enum class MessageSendMode
{
  eReliable = k_nSteamNetworkingSend_ReliableNoNagle,
  eReliableBatched = k_nSteamNetworkingSend_Reliable,
  eUnreliable = k_nSteamNetworkingSend_UnreliableNoNagle,
  eUnreliableBatched = k_nSteamNetworkingSend_Unreliable
};
enum class ConnectionState
{
  eNone = k_ESteamNetworkingConnectionState_None,
  eConnecting = k_ESteamNetworkingConnectionState_Connecting,
  eConnected = k_ESteamNetworkingConnectionState_Connected,
  eClosedByPeer = k_ESteamNetworkingConnectionState_ClosedByPeer,
  eProblemDetectedLocally =
      k_ESteamNetworkingConnectionState_ProblemDetectedLocally
};
using MessagePriority = JobPriority;
class MessageSystem : public System<MessageSystem>
{
public:
  class Connection
  {
    MessageSystem& system;
    HSteamNetConnection handle;
    friend class MessageSystem;

  protected:
    Connection(MessageSystem& system, HSteamNetConnection handle)
        : system(system), handle(handle)
    {
    }

  public:
    HSteamNetConnection GetHandle() const
    {
      return handle;
    }

    void SendMessage(const void* data, uint32_t size,
                     MessageSendMode mode) const
    {
      system.GNS().SendMessageToConnection(handle, data, size,
                                           static_cast<int>(mode), nullptr);
    }
  };
  class ListenSocketHandle
  {
    MessageSystem& system;
    HSteamListenSocket handle;
    friend class MessageSystem;

  protected:
    ListenSocketHandle(MessageSystem& system, HSteamListenSocket handle)
        : system(system), handle(handle)
    {
    }

  public:
    template <typename MessageType, typename Func>
      requires std::is_base_of_v<IMessage, MessageType>
    ListenSocketHandle& On(Func&& func)
    {
      return *this;
    }
  };

  MessageSystem();

  void SetIdentity(const SteamNetworkingIdentity& identity)
  {
    GNS().ResetIdentity(&identity);
  }

  ~MessageSystem()
  {
    if (!shutdown)
    {
      std::cerr << "MessageSystem destructor called without shutdown. Forcing shutdown now...\n";
      Shutdown();
    }
  }
  void Shutdown() override {
    if (!shutdown) return;
    shutdown = true;
    std::cerr << "Shutting down MessageSystem..." << std::endl;
    {
      std::unique_lock lock(_mutex);
      for (const auto& [address, connection] : _connections)
      {
        std::cerr << "Closing connection to " << address.to_string()
                  << std::endl;
        GNS().CloseConnection(connection.GetHandle(), 0, "Shutting down",
                              false);
      }
      _connections.clear();

      for (const auto& [port, listenSocket] : _listenSockets)
      {
        std::cerr << "Closing listen socket on port " << port << std::endl;
        GNS().CloseListenSocket(listenSocket.handle);
      }
      _listenSockets.clear();

      GameNetworkingSockets_Kill();
    }
    std::cerr << "MessageSystem shutdown complete." << std::endl;
  }

  JobHandle Connect(const EndPointAddress& address);
  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  void SendMessage(const MessageType& message, const EndPointAddress& address,
                   MessageSendMode mode,
                   MessagePriority priority = MessagePriority::eMedium)

  {
    Job SendMessageJob =
        Job(
            [this, message = message, address, mode](Job::JobContext& handle)
            {
              HSteamNetConnection con = GetConnectionHandle(address);
              ByteWriter bw;
              message.Serialize(bw);
              const auto data_span = bw.bytes();

              GNS().SendMessageToConnection(
                  con, data_span.data(),
                  static_cast<uint32_t>(data_span.size()),
                  static_cast<int>(mode), nullptr);
            })
            .set_priority(priority)
            .set_notify_level(JobNotifyLevel::eOnStart)
            .set_name(std::format("MessageSystem::SendMessage to {}",
                                  address.to_string()));

    if (_connections.contains(address))
    {
      std::cerr << "Connection exists, Pushing Send Job\n";
      JobSystem::Get().PushJob(SendMessageJob);
    }
    else
    {
      std::cerr << "Connection does not exist, Pushing Connect Job\n";
      JobHandle connectHandle = Connect(address);
      connectHandle.on_complete(SendMessageJob);
    }
  }

  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  MessageSystem& On(std::function<void(const MessageType&)> handler)
  {
    return *this;
  }

  ListenSocketHandle& OpenListenSocket(PortType port);
  ListenSocketHandle& GetListenSocket(PortType port);

  void SteamNetConnectionStatusChanged(
      SteamNetConnectionStatusChangedCallback_t* pInfo);

  ConnectionState GetConnectionState(const EndPointAddress& address) const;
  
  bool IsConnectingTo(const EndPointAddress& address) const;
  bool IsConnectedTo(const EndPointAddress& address) const;

  size_t GetNumConnections() const
  {
    std::shared_lock lock(_mutex);
    return _connections.size(); 
  }
  
private:
  ISteamNetworkingSockets& GNS() const
  {
    AN_ASSERT(_GNS, "GNS is null");
    return *_GNS;
  }
  HSteamNetConnection GetConnectionHandle(const EndPointAddress& address) const
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

  ISteamNetworkingSockets* _GNS;
  HSteamNetPollGroup _pollGroup;
  mutable std::shared_mutex _mutex;
  std::unordered_map<PortType, ListenSocketHandle> _listenSockets;
  std::unordered_map<EndPointAddress, Connection> _connections;
  std::unordered_map<EndPointAddress, JobHandle> _connectJobs;
  std::atomic_bool shutdown = false;
};
static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info)
{
  AtlasNet::MessageSystem::Get().SteamNetConnectionStatusChanged(info);
}
}; // namespace AtlasNet

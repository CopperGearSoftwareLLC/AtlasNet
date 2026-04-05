#pragma once
#include "Message.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/job/JobEnums.hpp"
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/job/JobOptions.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include <atomic>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <shared_mutex>
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
class MessageSystem
{
public:
  class Connection
  {
    MessageSystem& system;
    HSteamNetConnection handle;
    ConnectionState state;
    friend class MessageSystem;

  protected:
    Connection(MessageSystem& system, HSteamNetConnection handle)
        : system(system), handle(handle), state(ConnectionState::eNone)
    {
    }

  public:
    HSteamNetConnection GetHandle() const
    {
      return handle;
    }
    ConnectionState GetState() const
    {
      return state;
    }
    void SendMessage(const void* data, uint32_t size,
                     MessageSendMode mode) const;
  };
  class ListenSocketHandle
  {
    MessageSystem& system;
    HSteamListenSocket handle;
    std::shared_mutex socket_mutex;
    PortType port;
    using HandlerFunc =
        std::function<void(const IMessage&, const SocketAddress&)>;
    std::unordered_map<MessageIDHash, HandlerFunc> _handlers;
    // using DispatchFunc = std::function<void(const IMessage&, MessageIDHash,
    //                                         const SocketAddress&)>;
    // std::unordered_map<MessageIDHash, DispatchFunc> _dispatchTable;
    friend class MessageSystem;

  protected:
    void DispatchCallbacks(const IMessage& message, MessageIDHash typeIdHash,
                           const SocketAddress& caller_address);

  public:
    ListenSocketHandle(MessageSystem& system, HSteamListenSocket handle,
                       PortType port);
    template <typename MessageType>
      requires std::is_base_of_v<IMessage, MessageType>
    ListenSocketHandle&
    On(std::function<void(const MessageType&, const SocketAddress&)> func);

  private:
    /*  template <typename MsgType>
       requires std::is_base_of_v<IMessage, MsgType>
     void _ensure_socket_message_dispatcher(); */
  };
  struct Config
  {
    JobSystem* jobSystem = nullptr;
  };
  MessageSystem(const Config& config);

  void SetIdentity(const SteamNetworkingIdentity& identity);

  ~MessageSystem();
  void Shutdown();

  JobHandle Connect(const SocketAddress& address);
  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  [[nodiscard]] JobHandle
  SendMessage(const MessageType& message, const SocketAddress& address,
              MessageSendMode mode,
              MessagePriority priority = MessagePriority::eMedium);

  template <typename MessageType>
    requires std::is_base_of_v<IMessage, MessageType>
  MessageSystem&
  On(std::function<void(const MessageType&, const SocketAddress&)> handler);

  ListenSocketHandle& OpenListenSocket(PortType port);
  ListenSocketHandle& GetListenSocket(PortType port);

  void SteamNetConnectionStatusChanged(
      SteamNetConnectionStatusChangedCallback_t* pInfo);

  ConnectionState GetConnectionState(const SocketAddress& address) const;

  bool IsConnectingTo(const SocketAddress& address) const;
  bool IsConnectedTo(const SocketAddress& address) const;

  size_t GetNumConnections() const;
  void GetConnections(
      std::unordered_map<SocketAddress, Connection>& connections) const;
  std::optional<Connection> GetConnection(const SocketAddress& address) const;

private:
  void _Connect_to_job(JobContext& handle, const SocketAddress& address);
  void _Parse_Incoming_Messages();
  ISteamNetworkingSockets& GNS() const
  {
    AN_ASSERT(_GNS, "GNS is null");
    return *_GNS;
  }
  HSteamNetConnection GetConnectionHandle(const SocketAddress& address) const;
  template <typename MsgType>
    requires std::is_base_of_v<IMessage, MsgType>
  void _ensure_message_dispatcher();

  const Config config_;
  ISteamNetworkingSockets* _GNS;
  HSteamNetPollGroup _pollGroup;
  mutable std::shared_mutex _mutex;
  std::unordered_map<PortType, std::unique_ptr<ListenSocketHandle>>
      _listenSockets;
  std::unordered_map<SocketAddress, Connection> _connections;
  std::unordered_map<SocketAddress, JobHandle> _connectJobs;
  using HandlerFunc =
      std::function<void(const IMessage&, const SocketAddress&)>;
  std::unordered_map<MessageIDHash, HandlerFunc> _handlers;
  using DispatchFunc = std::function<void(ByteReader&, const SocketAddress&,
                                          std::optional<PortType>)>;
  std::unordered_map<MessageIDHash, DispatchFunc> _dispatchTable;
  std::optional<JobHandle> _pollJobHandle;
  std::atomic_bool shutdown = false;
};
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem::ListenSocketHandle& MessageSystem::ListenSocketHandle::On(
    std::function<void(const MessageType&, const SocketAddress&)> func)
{
  system._ensure_message_dispatcher<MessageType>();
  //_ensure_socket_message_dispatcher<MessageType>();
  std::cout << std::format("Registered handler for message type with hash {} "
                           "on listen socket port {}\n",
                           MessageType::TypeIdHash, port)
            << std::endl;
  std::unique_lock lock(socket_mutex);
  MessageIDHash typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] =
      [h = std::move(func)](const IMessage& msg, const SocketAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}
/*
template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void
MessageSystem::ListenSocketHandle::_ensure_socket_message_dispatcher()
{
  MessageIDHash typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(socket_mutex);
    if (_handlers.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(socket_mutex);

  if (!_handlers.contains(typeIdHash))
  {
    _handlers[typeIdHash] = [&](const IMessage& message,

                                     const SocketAddress& caller_address)
    {

      if (_handlers.contains(typeIdHash))
      {

        _handlers[typeIdHash](static_cast<const MsgType&>(message),
                              caller_address);
      }
      else
      {
        std::cerr << std::format(
            "No handler registered for message type with hash {}", typeIdHash);
      }
    };
  };
}
 */
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline MessageSystem& MessageSystem::On(
    std::function<void(const MessageType&, const SocketAddress&)> handler)
{
  _ensure_message_dispatcher<MessageType>();
  std::cout << std::format("Registered handler for message type with hash {} "
                           "on global message system\n",
                           MessageType::TypeIdHash)
            << std::endl;
  std::unique_lock lock(_mutex);
  MessageIDHash typeIdHash = MessageType::TypeIdHash;
  AN_ASSERT(
      !_handlers.contains(typeIdHash),
      std::format("Handler already registered for message type with hash {}",
                  typeIdHash));
  _handlers[typeIdHash] =
      [h = std::move(handler)](const IMessage& msg,
                               const SocketAddress& address)
  { h(static_cast<const MessageType&>(msg), address); };

  return *this;
}

template <typename MsgType>
  requires std::is_base_of_v<IMessage, MsgType>
inline void MessageSystem::_ensure_message_dispatcher()
{
  MessageIDHash typeIdHash = MsgType::TypeIdHash;
  {
    std::shared_lock lock(_mutex);
    if (_dispatchTable.contains(typeIdHash))
      return;
  }

  std::unique_lock lock(_mutex);

  if (!_dispatchTable.contains(typeIdHash))
  {
    _dispatchTable[typeIdHash] =
        [this](ByteReader& reader, const SocketAddress& address,
               std::optional<PortType> port_received_on)
    {
      MessageIDHash typeIdHash = MsgType::TypeIdHash;
      MsgType msg;
      msg.Deserialize(reader);
      std::shared_lock lock(_mutex);
      if (_handlers.contains(typeIdHash))
      {

        _handlers[typeIdHash](msg, address); // Placeholder for actual address
      }
      else
      {
        for (const auto& handler : _handlers)
        {
          std::cerr << std::format(
              "Registered handler for message type with hash {} does not match "
              "incoming message of type hash {}\n",
              handler.first, typeIdHash);
        }
      }
      if (port_received_on.has_value())
      {
        if (_listenSockets.contains(*port_received_on))
        {
          std::cerr
              << std::format(
                     "Dispatching message of type hash {} received on listen "
                     "socket port {} to listen socket dispatcher\n",
                     typeIdHash, *port_received_on)
              << std::endl;
          _listenSockets.at(*port_received_on)
              ->DispatchCallbacks(msg, typeIdHash, address);
        }
        else
        {
          std::cerr << std::format(
              "No listen socket found for incoming message of type hash {} "
              "received on listen socket port {}\n",
              typeIdHash, *port_received_on);
        }
      }
    };
  };
}
template <typename MessageType>
  requires std::is_base_of_v<IMessage, MessageType>
inline JobHandle MessageSystem::SendMessage(const MessageType& message,
                                            const SocketAddress& address,
                                            MessageSendMode mode,
                                            MessagePriority priority)
{
  auto sendFunc = [this, message, address, mode](JobContext&)
  {
    // Do not hold _mutex before calling GetConnectionHandle,
    // because GetConnectionHandle already locks internally.
    const HSteamNetConnection con = GetConnectionHandle(address);

    ByteWriter bw;
    message.Serialize(bw);
    const auto data = bw.bytes();

    const EResult result = GNS().SendMessageToConnection(
        con, data.data(), static_cast<uint32_t>(data.size()),
        static_cast<int>(mode), nullptr);

    if (result != k_EResultOK)
    {
      std::cerr << std::format(
                       "SendMessageToConnection failed for {} with code {}",
                       address.to_string(), static_cast<int>(result))
                << std::endl;
    }
  };

  enum class SendPlan
  {
    eSendNow,
    eChainToExistingConnect,
    eStartConnectThenChain
  };

  SendPlan plan = SendPlan::eStartConnectThenChain;
  std::optional<JobHandle> existingConnectHandle;

  {
    std::shared_lock lock(_mutex);

    auto connIt = _connections.find(address);
    const bool connected =
        (connIt != _connections.end() &&
         connIt->second.GetState() == ConnectionState::eConnected);

    if (connected)
    {
      plan = SendPlan::eSendNow;
    }
    else
    {
      auto connectIt = _connectJobs.find(address);
      if (connectIt != _connectJobs.end())
      {
        existingConnectHandle = connectIt->second;
        plan = SendPlan::eChainToExistingConnect;
      }
      else
      {
        plan = SendPlan::eStartConnectThenChain;
      }
    }
  }

  if (plan == SendPlan::eSendNow)
  {
    auto jobHandle = config_.jobSystem->Submit(
        sendFunc,
        JobOpts::Name(std::format("MessageSystem::SendMessage to {}",
                                  address.to_string())),
        AtlasNet::JobOpts::Priority(priority),
        AtlasNet::JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
    return jobHandle;
  }

  if (plan == SendPlan::eChainToExistingConnect)
  {
    auto jobhandle = existingConnectHandle->on_complete(
        sendFunc,
        JobOpts::Name(
            std::format("MessageSystem::SendMessage to {} after connect",
                        address.to_string())),
        AtlasNet::JobOpts::Priority(priority),
        AtlasNet::JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
    return jobhandle;
  }

  JobHandle connectHandle = Connect(address);
  JobHandle SendHandle = connectHandle.on_complete(
      sendFunc,
      JobOpts::Name(
          std::format("MessageSystem::SendMessage to {} after connect",
                      address.to_string())),
      AtlasNet::JobOpts::Priority(priority),
      AtlasNet::JobOpts::Notify<JobNotifyLevel::eOnStartAndComplete>{});
  return SendHandle;
}

static void OnSteamNetConnectionStatusChanged(
    SteamNetConnectionStatusChangedCallback_t* info)
{
  int64_t ptr_value = (info->m_info.m_nUserData);
  AN_ASSERT(
      ptr_value != 0 && ptr_value != ~0ll,
      std::format(
          "Invalid pointer value in OnSteamNetConnectionStatusChanged: {}",
          ptr_value));
  MessageSystem* system =
      reinterpret_cast<MessageSystem*>(info->m_info.m_nUserData);

  system->SteamNetConnectionStatusChanged(info);
}
}; // namespace AtlasNet

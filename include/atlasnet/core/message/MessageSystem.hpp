#pragma once
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/container/Container.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "steam/isteamnetworkingsockets.h"
#include "steam/steamnetworkingsockets.h"
#include "steam/steamnetworkingtypes.h"
#include "Message.hpp"
namespace AtlasNet
{
  enum class MessageSendMode
  {
    eReliable = k_nSteamNetworkingSend_ReliableNoNagle,
    eReliableBatched = k_nSteamNetworkingSend_Reliable,
    eUnreliable = k_nSteamNetworkingSend_UnreliableNoNagle,
    eUnreliableBatched = k_nSteamNetworkingSend_Unreliable
  };
  class MessageSystem : System<MessageSystem>
  {
  public:
    class Connection
    {
      MessageSystem& system;
      HSteamNetConnection handle;

    protected:
      Connection(MessageSystem& system, HSteamNetConnection handle)
          : system(system)
          , handle(handle)
      {
      }

    public:
      HSteamNetConnection GetHandle() const
      {
        return handle;
      }

      void SendMessage(const void* data, uint32_t size, MessageSendMode mode) const
      {

        system.GNS().SendMessageToConnection(handle, data, size, static_cast<int>(mode), nullptr);
      }
    };

  protected:
    MessageSystem()
    {
      ContainerID ID = ContainerInfo::Get().GetID();
      SteamNetworkingErrMsg errMsg;
      bool result = GameNetworkingSockets_Init(nullptr, errMsg);
      if (!result)
      {
        throw std::runtime_error(
            std::string("Failed to initialize GameNetworkingSockets: ") + errMsg
        );
      }
      _GNS = SteamNetworkingSockets();
    }
    ISteamNetworkingSockets& GNS()
    {
      return *_GNS;
    }

    void SetIdentity(const SteamNetworkingIdentity& identity)
    {
      GNS().ResetIdentity(&identity);
    }

  public:
    template <typename MessageType>
    void
    SendMessage(const MessageType& message, const EndPointAddress& address, MessageSendMode mode)
    {
    }

    void OpenListenSocket(PortType port)
    {
      if (_listenSocket.has_value())
      {
        throw std::runtime_error("Listen socket already open");
      }
      SteamNetworkingIPAddr localAddr;
      localAddr.Clear();
      localAddr.m_port = port;
      _listenSocket = GNS().CreateListenSocketIP(localAddr, 0,nullptr);
      if (_listenSocket == k_HSteamListenSocket_Invalid)
      {
        throw std::runtime_error("Failed to create listen socket");
      }
    }

  private:
    ISteamNetworkingSockets* _GNS;
    std::optional<HSteamListenSocket> _listenSocket;
  };
}; // namespace AtlasNet

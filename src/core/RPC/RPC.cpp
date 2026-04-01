#include "atlasnet/core/RPC/RPC.hpp"
#include "atlasnet/core/RPC/RPCMessage.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"

AtlasNet::RPC::RPC(const Settings& settings)
    : _settings(settings)
{
  MessageSystem::Get()
      .OpenListenSocket(settings.port)
      .On<RpcRequestMessage>(
          [this](const RpcRequestMessage& msg, const EndPointAddress& address)
          { OnRPCRequest(msg, address); }
      )
      .On<RpcResponseMessage>(
          [this](const RpcResponseMessage& msg, const EndPointAddress& address)
          { OnRPCResponse(msg, address); }
      )
      .On<RpcErrorMessage>(
          [this](const RpcErrorMessage& msg, const EndPointAddress& address)
          { OnRPCError(msg, address); }
      );
}


void AtlasNet::RPC::OnRPCRequest(
    const RpcRequestMessage& msg, const EndPointAddress& address
)
{
}
void AtlasNet::RPC::Shutdown() {}

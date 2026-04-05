#include "atlasnet/core/RPC/RPC.hpp"
#include "atlasnet/core/RPC/RPCMessage.hpp"
#include "atlasnet/core/assert.hpp"
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include <iostream>
#include <shared_mutex>
#include <utility>

AtlasNet::RPC::RPC(const Config& config) : config_(config)
{
  AN_ASSERT(config_.messageSystem != nullptr,
            "RPC requires a valid MessageSystem");

  config_.messageSystem->OpenListenSocket(config.port)
      .On<RpcRequestMessage>(
          [this](const RpcRequestMessage& msg, const SocketAddress& address)
          {
            OnRPCRequest(msg, address);
          }); // requests should be handled by the listen socket callback to
              // ensure we know which port they came in on


  config_.messageSystem
      ->On<RpcResponseMessage>(
          [this](const RpcResponseMessage& msg, const SocketAddress& address)
          { OnRPCResponse(msg, address); })
      .On<RpcErrorMessage>(
          [this](const RpcErrorMessage& msg, const SocketAddress& address)
          { OnRPCError(msg, address); });
}

void AtlasNet::RPC::OnRPCRequest(const RpcRequestMessage& msg,
                                 const SocketAddress& address)
{
    std::cerr << std::format("Received RPC request for methodId {} callId {} from {}",
                             msg.methodId, msg.callID, address.to_string())
              << std::endl;
  BindFunc handler;
  {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _methodBindHandlers.find(msg.methodId);
    if (it == _methodBindHandlers.end())
    {
      return;
    }
    handler = it->second;
  }

  handler(address, msg.callID, msg.payload);
}

void AtlasNet::RPC::Shutdown() {}

void AtlasNet::RPC::SendError(const RPCTarget& target,
                              RPC_Internal::MethodID methodId,
                              RPC_Internal::CallID callID, std::string errorMsg)
{
  RpcErrorMessage error{
      .methodId = methodId, .callID = callID, .ErrorMsg = std::move(errorMsg)};

  JobHandle sendErrorHandle =  config_.messageSystem->SendMessage(error, target,
                                     MessageSendMode::eReliableBatched);

    //NewActiveJob(sendErrorHandle);
}

void AtlasNet::RPC::OnRPCError(const RpcErrorMessage& msg,
                               const SocketAddress& address)
{
    std::cerr << std::format("Received RPC error for methodId {} callId {} from {}: {}",
                             msg.methodId, msg.callID, address.to_string(), msg.ErrorMsg)
              << std::endl;
  (void)address;

  PendingRequest pending;
  {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it =
        _pendingPromises.find(PendingPromiseKey{msg.methodId, msg.callID});
    if (it == _pendingPromises.end())
    {
      return;
    }

    pending = std::move(it->second);
    _pendingPromises.erase(it);
  }

  if (pending.onError)
  {
    pending.onError(msg.ErrorMsg);
  }
}

void AtlasNet::RPC::OnRPCResponse(const RpcResponseMessage& msg,
                                  const SocketAddress& address)
{
    std::cerr << std::format("Received RPC response for methodId {} callId {} from {}",
                             msg.methodId, msg.callID, address.to_string())
              << std::endl;
  (void)address;

  std::cerr << std::format("Received RPC response for methodId {} callId {}",
                           msg.methodId, msg.callID)
            << std::endl;
  PendingRequest pending;
  {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it =
        _pendingPromises.find(PendingPromiseKey{msg.methodId, msg.callID});
    if (it == _pendingPromises.end())
    {
      std::cerr << std::format("No pending promise found for RPC response with "
                               "methodId {} callId {}",
                               msg.methodId, msg.callID)
                << std::endl;
      return;
    }

    pending = std::move(it->second);
    _pendingPromises.erase(it);
  }

  if (pending.onResponse)
  {
    pending.onResponse(msg.payload);
  }
}

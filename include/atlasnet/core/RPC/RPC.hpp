#pragma once

#include "atlasnet/core/RPC/RPCMessage.hpp"

#include "RPCConcepts.hpp"
#include "atlasnet/core/job/JobHandle.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "boost/describe/enum_to_string.hpp"
#include <functional>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <stack>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#define ATLASNET_RPC_METHOD(Name, ReturnType, ...)                             \
  struct Name                                                                  \
      : AtlasNet::RPC_Internal::Method<AtlasNet::RPC_Internal::Fnv1a32(#Name), \
                                       ReturnType(__VA_ARGS__)>                \
  {                                                                            \
    static constexpr const char* GetName()                                     \
    {                                                                          \
      return #Name;                                                            \
    }                                                                          \
  };

#define ATLASNET_RPC(ServiceName, ...)                                         \
  struct ServiceName                                                           \
  {                                                                            \
    __VA_ARGS__                                                                \
  };

namespace AtlasNet
{

using RPCTarget = SocketAddress;
class RPC
{
public:
  struct Config
  {
    PortType port;
    MessageSystem* messageSystem = nullptr;
  };

  RPC(const Config& config);
  ~RPC()
  {
    /* {
      std::cerr << "RPC destructor called, shutting down RPC and waiting for "
                   "active jobs to complete..."
                << std::endl;
      std::unique_lock u(_activeJobsMutex);
      while (!activeJobs.empty())
      {
        if (!activeJobs.top().is_completed())
        {
          std::cerr << "Waiting for active job "
                    << activeJobs.top().name().value_or("<unnamed>")
                    << " to complete... Current State: "
                    << boost::describe::enum_to_string(activeJobs.top().state(),
                                                       "UNKNOWN")
                    << std::endl;
          activeJobs.top().wait();
        }

        activeJobs.pop();
      }
      std::cerr << "RPC destructor  done." << std::endl;
    } */
  }

  template <typename MethodType, typename Func>
    requires RPC_Internal::BindableRpcHandler<MethodType, Func>
  void Bind(Func&& func);

  template <typename MethodType, typename... Args>
    requires(!std::is_void_v<typename MethodType::ReturnType>)
  [[nodiscard]] std::future<typename MethodType::ReturnType>
  Call(const RPCTarget& target, Args&&... args);

  template <typename MethodType, typename... Args>
    requires(std::is_void_v<typename MethodType::ReturnType>)
  void Call(const RPCTarget& target, Args&&... args);

private:
  void Shutdown();

  template <typename MethodType, typename... Args>
  std::pair<RPC_Internal::MethodID, RPC_Internal::CallID>
  SendRequest(const RPCTarget& target, Args&&... args);

  template <typename MethodType>
  void SendResponse(const RPCTarget& target, RPC_Internal::CallID callID,
                    const typename MethodType::ReturnType& ret);

  void SendError(const RPCTarget& target, RPC_Internal::MethodID methodId,
                 RPC_Internal::CallID callID, std::string errorMsg);

  void OnRPCRequest(const RpcRequestMessage& msg,
                    const SocketAddress& address);
  void OnRPCResponse(const RpcResponseMessage& msg,
                     const SocketAddress& address);
  void OnRPCError(const RpcErrorMessage& msg, const SocketAddress& address);

  RPC_Internal::CallID GetNextCallID(RPC_Internal::MethodID methodId)
  {
    auto& next = _nextCallID[methodId];
    return next++;
  }
  /* void NewActiveJob(JobHandle handle)
  {
    std::unique_lock u(_activeJobsMutex);
    for (int i = 0; i < activeJobs.size(); ++i)
    {
      if (activeJobs.top().is_completed())
      {
        activeJobs.pop();
      }
    }
    activeJobs.push(handle);
  } */
  struct PendingPromiseKey
  {
    RPC_Internal::MethodID methodId;
    RPC_Internal::CallID callId;

    bool operator==(const PendingPromiseKey& other) const noexcept
    {
      return methodId == other.methodId && callId == other.callId;
    }
  };

  struct PendingPromiseKeyHash
  {
    std::size_t operator()(const PendingPromiseKey& k) const noexcept
    {
      std::size_t h1 = std::hash<RPC_Internal::MethodID>{}(k.methodId);
      std::size_t h2 = std::hash<RPC_Internal::CallID>{}(k.callId);
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  using BindFunc = std::function<void(const RPCTarget&, RPC_Internal::CallID,
                                      std::span<const uint8_t>)>;

  struct PendingRequest
  {
    std::function<void(std::span<const uint8_t>)> onResponse;
    std::function<void(const std::string&)> onError;
  };

  std::unordered_map<RPC_Internal::MethodID, BindFunc> _methodBindHandlers;
  std::unordered_map<RPC_Internal::MethodID, RPC_Internal::CallID> _nextCallID;
  std::unordered_map<PendingPromiseKey, PendingRequest, PendingPromiseKeyHash>
      _pendingPromises;
  //std::shared_mutex _activeJobsMutex;
  //std::stack<JobHandle> activeJobs;

  std::shared_mutex _mutex;
  const Config config_;
};

} // namespace AtlasNet

template <typename MethodType, typename... Args>
inline std::pair<AtlasNet::RPC_Internal::MethodID,
                 AtlasNet::RPC_Internal::CallID>
AtlasNet::RPC::SendRequest(const RPCTarget& target, Args&&... args)
{
  ByteWriter writeArgs;
  writeArgs(std::forward<Args>(args)...);

  RpcRequestMessage request{.methodId = MethodType::Id,
                            .callID = GetNextCallID(MethodType::Id),
                            .payload =
                                std::vector<uint8_t>(writeArgs.bytes().begin(),
                                                     writeArgs.bytes().end())};

  JobHandle sendRequestHandle = config_.messageSystem->SendMessage(
      request, target, MessageSendMode::eReliableBatched);
  //NewActiveJob(sendRequestHandle);
  return std::make_pair(request.methodId, request.callID);
}

template <typename MethodType>
inline void
AtlasNet::RPC::SendResponse(const RPCTarget& target,
                            RPC_Internal::CallID callID,
                            const typename MethodType::ReturnType& ret)
{
  ByteWriter writeArgs;
  writeArgs(ret);

  RpcResponseMessage response{
      .methodId = MethodType::Id,
      .callID = callID,
      .payload = std::vector<uint8_t>(writeArgs.bytes().begin(),
                                      writeArgs.bytes().end())};
  std::cerr << std::format(
                   "Sending RPC response for methodId {} callId {} to {}",
                   response.methodId, response.callID, target.to_string())
            << std::endl;
  JobHandle sendResponseHandle = config_.messageSystem->SendMessage(
      response, target, MessageSendMode::eReliableBatched);
  //NewActiveJob(sendResponseHandle);
}

template <typename MethodType, typename... Args>
  requires(std::is_void_v<typename MethodType::ReturnType>)
inline void AtlasNet::RPC::Call(const RPCTarget& target, Args&&... args)
{
  SendRequest<MethodType>(target, std::forward<Args>(args)...);
}

template <typename MethodType, typename... Args>
  requires(!std::is_void_v<typename MethodType::ReturnType>)
std::future<typename MethodType::ReturnType>
AtlasNet::RPC::Call(const RPCTarget& target, Args&&... args)
{
  using ReturnType = typename MethodType::ReturnType;

  auto promise = std::make_shared<std::promise<ReturnType>>();
  auto future = promise->get_future();

  RPC_Internal::MethodID methodId = MethodType::Id;
  RPC_Internal::CallID callID;

  {
    std::unique_lock lock(_mutex);
    callID = GetNextCallID(methodId);

    PendingRequest pending;
    pending.onResponse = [promise](std::span<const uint8_t> payload) mutable
    {
      try
      {
        ByteReader reader(payload);
        ReturnType value{};
        reader(value);
        promise->set_value(std::move(value));
      }
      catch (...)
      {
        promise->set_exception(std::current_exception());
      }
    };

    pending.onError = [promise](const std::string& errorMsg) mutable
    {
      promise->set_exception(
          std::make_exception_ptr(std::runtime_error(errorMsg)));
    };

    _pendingPromises.emplace(PendingPromiseKey{methodId, callID},
                             std::move(pending));
  }

  ByteWriter writeArgs;
  writeArgs(std::forward<Args>(args)...);

  RpcRequestMessage request{.methodId = methodId,
                            .callID = callID,
                            .payload =
                                std::vector<uint8_t>(writeArgs.bytes().begin(),
                                                     writeArgs.bytes().end())};

  config_.messageSystem->SendMessage(request, target,
                                     MessageSendMode::eReliableBatched);

  return future;
}

template <typename MethodType, typename Func>
  requires AtlasNet::RPC_Internal::BindableRpcHandler<MethodType, Func>
inline void AtlasNet::RPC::Bind(Func&& func)
{
  using ReturnType = typename MethodType::ReturnType;
  using ArgsTuple = typename MethodType::ArgsTuple;

  std::unique_lock lock(_mutex);
  _methodBindHandlers[MethodType::Id] =
      [f = std::forward<Func>(func),
       this](const RPCTarget& caller, RPC_Internal::CallID callId,
             std::span<const uint8_t> payload) mutable
  {
    /*  try
     { */
    ByteReader reader(payload);

    ArgsTuple args;
    std::apply([&](auto&... arg) { (reader(arg), ...); }, args);

    if constexpr (std::is_void_v<ReturnType>)
    {
      std::apply(f, args);
    }
    else
    {
      ReturnType result = std::apply(f, args);
      SendResponse<MethodType>(caller, callId, result);
    }
    /* }
    catch (const std::exception& e)
    {
      SendError(caller, MethodType::Id, callId, e.what());
    }
    catch (...)
    {
      SendError(caller, MethodType::Id, callId, "Unhandled RPC exception");
    } */
  };
}

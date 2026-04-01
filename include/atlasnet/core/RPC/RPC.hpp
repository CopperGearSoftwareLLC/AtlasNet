#pragma once

#include "atlasnet/core/RPC/RPCMessage.hpp"
#include "atlasnet/core/System.hpp"
#include "atlasnet/core/database/RedisConn.hpp"

#include "RPCConcepts.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/serialize/ByteReader.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "steam/steamtypes.h"
#include <any>
#include <functional>
#include <future>
#include <tuple>
#include <unordered_map>
#define ATLASNET_RPC_METHOD(Name, ReturnType, ...)                             \
  struct Name : AtlasNet::RPC_Internal::Method<                                \
                    AtlasNet::RPC_Internal::Fnv1a32(#Name),                    \
                    ReturnType(__VA_ARGS__)>                                   \
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

  using RPCTarget = EndPointAddress;
  class RPC : public System<RPC>
  {

  public:
    struct Settings
    {
      PortType port;
    } _settings;

    RPC(const Settings& settings);
    ~RPC() {}
    /**
     * @brief Hello
     *
     * @tparam MethodType
     * @tparam Func
     * @param func
     * @return requires
     */
    template <typename MethodType, typename Func>
      requires RPC_Internal::BindableRpcHandler<MethodType, Func>
    // Bind the method to the RPC system
    void Bind(Func&& func);
    template <typename MethodType, typename... Args>
      requires(!std::is_void_v<typename MethodType::ReturnType>)
    [[nodiscard]]
    std::future<typename MethodType::ReturnType>
    Call(const RPCTarget& target, Args&&... args);

    template <typename MethodType, typename... Args>
      requires(std::is_void_v<typename MethodType::ReturnType>)
    void Call(const RPCTarget& target, Args&&... args);

  private:
    void Shutdown() override;
    template <typename MethodType, typename... Args>
    std::pair<RPC_Internal::MethodID, RPC_Internal::CallID>
    SendRequest(const RPCTarget& target, Args&&... args);
    template <typename MethodType>
    void SendResponse(
        const RPCTarget& target,
        RPC_Internal::CallID callID,
        const typename MethodType::Return& ret
    );
    void
    OnRPCRequest(const RpcRequestMessage& msg, const EndPointAddress& address);
    void OnRPCResponse(
        const RpcResponseMessage& msg, const EndPointAddress& address
    );
    void OnRPCError(const RpcErrorMessage& msg, const EndPointAddress& address);
    RPC_Internal::CallID GetNextCallID(RPC_Internal::MethodID methodId)
    {
      if (!NextCallID.contains(methodId))
      {
        NextCallID[methodId] = 0; // Start call IDs at 0 for each method
      }
      return NextCallID[methodId]++;
    }
    using BindFunc = std::function<
        void(const RPCTarget&, RPC_Internal::CallID, std::span<const uint8_t>)>;
    std::unordered_map<RPC_Internal::MethodID, BindFunc> _methodHandlers;
    std::unordered_map<RPC_Internal::MethodID, RPC_Internal::CallID> NextCallID;
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

    std::unordered_map<PendingPromiseKey, std::any, PendingPromiseKeyHash>
        _pendingPromises;
  };

} // namespace AtlasNet

template <typename MethodType, typename... Args>
inline std::
    pair<AtlasNet::RPC_Internal::MethodID, AtlasNet::RPC_Internal::CallID>
    AtlasNet::RPC::SendRequest(const RPCTarget& target, Args&&... args)
{
  ByteWriter writeArgs;
  writeArgs(std::forward<Args>(args)...);
  RpcRequestMessage request{
      .methodId = MethodType::Id,
      .callID = GetNextCallID(MethodType::Id),
      .payload = std::vector<uint8_t>(
          writeArgs.bytes().begin(), writeArgs.bytes().end()
      )
  };
  MessageSystem::Get().SendMessage(
      request, target, MessageSendMode::eReliableBatched
  );
  return std::make_pair(request.methodId, request.callID);
}

template <typename MethodType>
inline void AtlasNet::RPC::SendResponse(
    const RPCTarget& target,
    RPC_Internal::CallID callID,
    const MethodType::Return& ret
)
{
  ByteWriter writeArgs;
  writeArgs(ret);
  RpcResponseMessage request{
      .methodId = MethodType::Id,
      .callID = GetNextCallID(MethodType::Id),
      .payload = std::vector<uint8_t>(
          writeArgs.bytes().begin(), writeArgs.bytes().end()
      )
  };
  MessageSystem::Get().SendMessage(
      request, target, MessageSendMode::eReliableBatched
  );
}

template <typename MethodType, typename... Args>
  requires(std::is_void_v<typename MethodType::ReturnType>)
inline void AtlasNet::RPC::Call(const RPCTarget& target, Args&&... args)
{
  SendRequest<MethodType>(target, std::forward<Args>(args)...);
}

template <typename MethodType, typename... Args>
  requires(!std::is_void_v<typename MethodType::ReturnType>)
inline std::future<typename MethodType::ReturnType>
AtlasNet::RPC::Call(const RPCTarget& target, Args&&... args)
{
  auto [methodId, callID] =
      SendRequest<MethodType>(target, std::forward<Args>(args)...);
  PendingPromiseKey key = std::make_pair(methodId, callID);
  _pendingPromises.emplace(
      key, std::promise<std::optional<typename MethodType::ReturnType>>()
  );
  return std::any_cast<
             std::promise<std::optional<typename MethodType::ReturnType>>&>(
             _pendingPromises.at(key)
  )
      .get_future();
}

template <typename MethodType, typename Func>
  requires AtlasNet::RPC_Internal::BindableRpcHandler<MethodType, Func>
inline void AtlasNet::RPC::Bind(Func&& func)
{
  using ReturnType = typename MethodType::ReturnType;
  using ArgsTuple = typename MethodType::ArgsTuple;

  _methodHandlers[MethodType::Id] = [f = std::forward<Func>(func), this](
                                        const RPCTarget& caller,
                                        RPC_Internal::CallID CallId,
                                        std::span<const uint8_t> payload
                                    ) mutable
  {
    ByteReader reader(payload);

    ArgsTuple args;
    std::apply([&](auto&... arg) { (reader(arg), ...); }, args);

    if constexpr (std::is_void_v<ReturnType>)
    {
      std::apply(f, args);
      // SendVoidResponse(sender, MethodType::Id, callId);
    }
    else
    {
      ReturnType result = std::apply(f, args);
      /* RpcResponseMessage response;
       response.methodId = MethodType::Id;
       response.callID = CallId;
       ByteWriter bw;
       bw(result);
       response.payload.assign(bw.bytes().begin(), bw.bytes().end()); */
      SendResponse<MethodType>(caller, CallId, result);
    }
  };
}

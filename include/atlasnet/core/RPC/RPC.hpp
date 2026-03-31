#pragma once

#include "atlasnet/core/System.hpp"
#include "atlasnet/core/container/Container.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/message/MessageSystem.hpp"
#include <cstdint>
#include <future>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
namespace AtlasNet
{
  namespace RPC_Internal
  {

    template <typename Signature> struct MethodTraits;

    template <typename R, typename... Args> struct MethodTraits<R(Args...)>
    {
      using ReturnType = R;
      using ArgsTuple = std::tuple<std::decay_t<Args>...>;
      static constexpr bool IsVoid = std::is_void_v<R>;
    };

    template <uint32_t IdValue, typename Signature> struct Method : MethodTraits<Signature>
    {
      static constexpr uint32_t Id = IdValue;
    };
    template <typename ReturnType, typename Func, typename Tuple> struct IsInvocableFromTuple;

    template <typename ReturnType, typename Func, typename... Args>
    struct IsInvocableFromTuple<ReturnType, Func, std::tuple<Args...>>
        : std::bool_constant<std::is_invocable_r_v<ReturnType, Func, Args...>>
    {
    };

    template <typename MethodType, typename Func>
    concept BindableRpcHandler = IsInvocableFromTuple<
        typename MethodType::ReturnType,
        Func,
        typename MethodType::ArgsTuple>::value;
  } // namespace RPC_Internal
  using RPCTarget = std::variant<ContainerID, EndPointAddress>;
  class RPC : public System<RPC>
  {

  public:
    RPC()
    {
      // Initialize RPC system resources here
    }
    template <typename MethodType, typename Func>
    requires RPC_Internal::BindableRpcHandler<MethodType, Func>
        // Bind the method to the RPC system
        static void Bind(Func&& func)
    {
    }
    template <typename MethodType, typename... Args>
    requires(!std::is_void_v<typename MethodType::ReturnType>) [[nodiscard]]
    static std::future<typename MethodType::ReturnType> Call(
        const RPCTarget& target, Args&&... args
    )
    {
      // non-void RPC result must be used
    }

    template <typename MethodType, typename... Args>
    requires(std::is_void_v<typename MethodType::ReturnType>) static std::future<
        typename MethodType::ReturnType> Call(const RPCTarget& target, Args&&... args)
    {
      // void RPC result may be ignored
    }
  };

} // namespace AtlasNet
#define ATLASNET_RPC_METHOD(Name, Id, ReturnType, ...)                                             \
  struct Name : AtlasNet::RPC_Internal::Method<Id, ReturnType(__VA_ARGS__)>                        \
  {                                                                                                \
    static constexpr const char* GetName()                                                         \
    {                                                                                              \
      return #Name;                                                                                \
    }                                                                                              \
  };

#define ATLASNET_RPC(ServiceName, ...)                                                             \
  struct ServiceName                                                                               \
  {                                                                                                \
    __VA_ARGS__                                                                                    \
  };

namespace AtlasNet
{

  ATLASNET_RPC(
      TESTRpc,
      ATLASNET_RPC_METHOD(TestMethod, 1, void, int, float)
          ATLASNET_RPC_METHOD(SecondTestMethod, 2, int, std::string)
  );
  void Test()
  {
    auto test_method_bind = [](int a, float b) {};
    auto second_test_method_bind = [](const std::string_view) { return 1; };

    RPC::Bind<TESTRpc::TestMethod>(test_method_bind);
    RPC::Bind<TESTRpc::SecondTestMethod>(second_test_method_bind);

    RPCTarget target = EndPointAddress(IPv4Address(127, 0, 0, 1), 8080);
    RPCTarget target2 = ContainerID();

    RPC::Call<TESTRpc::TestMethod>(target, 42, 3.14f);
    std::future<int> second_result = RPC::Call<TESTRpc::SecondTestMethod>(target, "Hello");

    int result = second_result.get();
  }
} // namespace AtlasNet
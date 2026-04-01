#pragma once

 #include <cstdint>
#include <tuple>
namespace AtlasNet::RPC_Internal
  {
    using MethodID = uint32_t;
    using CallID = uint32_t;
    
    constexpr uint32_t Fnv1a32(const char* str)
    {
      uint32_t hash = 2166136261u;
      while (*str)
      {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
      }
      return hash;
    }

    template <typename Signature> struct MethodTraits;

    template <typename R, typename... Args> struct MethodTraits<R(Args...)>
    {
      using ReturnType = R;
      using ArgsTuple = std::tuple<std::decay_t<Args>...>;
      static constexpr bool IsVoid = std::is_void_v<R>;
    };

    template <MethodID IdValue, typename Signature> struct Method : MethodTraits<Signature>
    {
      static constexpr MethodID Id = IdValue;
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
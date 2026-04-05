#pragma once


#include "atlasnet/core/database/redis/RedisConn.hpp"

namespace AtlasNet::Database::Redis{


  class SetWrapper
  {
      friend class AtlasNet::Database::RedisConn;
    RedisConn& conn;
    protected:
    SetWrapper(RedisConn& c) : conn(c) {}

    class ModifyWrapper
    {
      friend class SetWrapper;
      SetWrapper& wrapper;
      ModifyWrapper(SetWrapper& w) : wrapper(w) {}

    public:
      bool SAdd(const std::string_view key, const std::string_view member)
      {
        return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                      { return handle.sadd(key, member) > 0; });
      }

      template <typename T>
      size_t SAdd(const std::string_view key, T first, T last)
      {
        return wrapper.conn.RedisFunc(
            [&](auto&& handle) -> size_t
            { return handle.sadd(key, first, last); });
      }

      template <typename T>
      size_t SAdd(const std::string_view key, std::initializer_list<T> members)
      {
        return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                      { return handle.sadd(key, members); });
      }

      bool SRem(const std::string_view key, const std::string_view member)
      {
        return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                      { return handle.srem(key, member) > 0; });
      }

      template <typename T>
      size_t SRem(const std::string_view key, T first, T last)
      {
        return wrapper.conn.RedisFunc(
            [&](auto&& handle) -> size_t
            { return handle.srem(key, first, last); });
      }

      template <typename T>
      size_t SRem(const std::string_view key, std::initializer_list<T> members)
      {
        return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                      { return handle.srem(key, members); });
      }

      std::optional<std::string> SPop(const std::string_view key)
      {
        return wrapper.conn.RedisFunc(
            [&](auto&& handle) -> std::optional<std::string>
            { return handle.spop(key); });
      }
    };

    class QueryWrapper
    {
      friend class SetWrapper;
      SetWrapper& wrapper;
      QueryWrapper(SetWrapper& w) : wrapper(w) {}

    public:
      bool SIsMember(const std::string_view key, const std::string_view member)
      {
        return wrapper.conn.RedisFunc(
            [&](auto&& handle) -> bool
            { return handle.sismember(key, member); });
      }

      size_t SCard(const std::string_view key)
      {
        return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                      { return handle.scard(key); });
      }

      template <typename Output>
      void SMembers(const std::string_view key, Output output)
      {
        wrapper.conn.RedisFunc(
            [&](auto&& handle) -> void
            { handle.smembers(key, std::forward<Output>(output)); });
      }
    };

  public:
    ModifyWrapper Modify()
    {
      return ModifyWrapper(*this);
    }
    QueryWrapper Query()
    {
      return QueryWrapper(*this);
    }
  };
}
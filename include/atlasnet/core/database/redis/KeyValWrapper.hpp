

#pragma once

#include "atlasnet/core/database/redis/RedisConn.hpp"
namespace AtlasNet::Database
{
namespace Redis
{
class KeyValWrapper
{
  friend class AtlasNet::Database::RedisConn;
  RedisConn& conn;
  protected:
  KeyValWrapper(RedisConn& c) : conn(c) {}
  class DeleteWrapper
  {
    friend class KeyValWrapper;
    KeyValWrapper& wrapper;
    DeleteWrapper(KeyValWrapper& w) : wrapper(w) {}

  public:
    /* Delete*/
    bool Del(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.del(key) > 0; });
    }
    template <typename T> bool Del(T first, T last)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.del(first, last) > 0; });
    }
    template <typename T> bool Del(std::initializer_list<T> keys)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.del(keys) > 0; });
    }
  };

  class ExistsWrapper
  {
    friend class KeyValWrapper;
    KeyValWrapper& wrapper;
    ExistsWrapper(KeyValWrapper& w) : wrapper(w) {}

  public:
    bool Exists(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.exists(key) > 0; });
    }
    template <typename T> bool Exists(std::initializer_list<T> keys)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.exists(keys) > 0; });
    }
    template <typename T> bool Exists(T first, T last)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.exists(first, last) > 0; });
    }
  };

  class ExpireWrapper
  {
    friend class KeyValWrapper;
    KeyValWrapper& wrapper;
    ExpireWrapper(KeyValWrapper& w) : wrapper(w) {}

  public:
    bool Expire(const std::string_view key, std::chrono::seconds ttl)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.expire(key, ttl); });
    }
    bool
    ExpireAt(const std::string_view key,
             const std::chrono::time_point<std::chrono::system_clock,
                                           std::chrono::seconds>& timestamp)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> bool
          { return handle.expireat(key, timestamp); });
    }
  };
  class GetSetWrapper
  {
    friend class KeyValWrapper;
    KeyValWrapper& wrapper;
    GetSetWrapper(KeyValWrapper& w) : wrapper(w) {}

  public:
    size_t Decr(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.decr(key); });
    }
    size_t DecrBy(const std::string_view key, size_t decrement)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.decrby(key, decrement); });
    }
    size_t Incr(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.incr(key); });
    }
    size_t IncrBy(const std::string_view key, size_t increment)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.incrby(key, increment); });
    }
    bool Set(const std::string_view key, const std::string_view value)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.set(key, value); });
    }
    std::optional<std::string> Get(const std::string_view key)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> std::optional<std::string>
          { return handle.get(key); });
    }
    template <typename Output>
    void Keys(std::string_view pattern, Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.keys(pattern, std::forward<Output>(output)); });
    }

    void Rename(const std::string_view oldKey, const std::string_view newKey)
    {
      wrapper.conn.RedisFunc([&](auto&& handle) -> void
                             { handle.rename(oldKey, newKey); });
    }
    bool Append(const std::string_view key, const std::string_view value)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.append(key, value) > 0; });
    }
  };
  class BitwiseWrapper
  {
    friend class KeyValWrapper;
    KeyValWrapper& wrapper;
    BitwiseWrapper(KeyValWrapper& w) : wrapper(w) {}

  public:
    size_t BitCount(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.bitcount(key); });
    }
    size_t Bitop(const sw::redis::BitOp operation,
                 const std::string_view destkey, const std::string_view key)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> size_t
          { return handle.bitop(operation, destkey, key); });
    }
    bool SetBit(const std::string_view key, size_t offset, bool value)
    {
      if (value)
      {
        sw::redis::BitOp op = sw::redis::BitOp::OR;
        return Bitop(op, key, key) > 0;
      }
      else
      {
        sw::redis::BitOp op = sw::redis::BitOp::AND;
        return Bitop(op, key, key) == 0;
      }
    }
    bool GetBit(const std::string_view key, size_t offset)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.getbit(key, offset); });
    }
  };

public:
  DeleteWrapper Delete()
  {
    return DeleteWrapper(*this);
  }
  ExistsWrapper Exists()
  {
    return ExistsWrapper(*this);
  }
  ExpireWrapper Expire()
  {
    return ExpireWrapper(*this);
  }
  GetSetWrapper GetSet()
  {
    return GetSetWrapper(*this);
  }
  BitwiseWrapper Bitwise()
  {
    return BitwiseWrapper(*this);
  }
};
} // namespace Redis

}; // namespace AtlasNet::Database
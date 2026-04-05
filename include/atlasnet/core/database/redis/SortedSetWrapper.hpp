#pragma once

#include "atlasnet/core/database/redis/RedisConn.hpp"
#include "sw/redis++/command_options.h"
#include "sw/redis++/redis.h"

namespace AtlasNet::Database::Redis
{

class SortedSetWrapper
{
  friend class AtlasNet::Database::RedisConn;
  RedisConn& conn;
public:
 enum class BoundType
    {
      eLeftOpen = (int)sw::redis::BoundType::LEFT_OPEN,
      eRightOpen = (int)sw::redis::BoundType::RIGHT_OPEN,
      eOpen = (int)sw::redis::BoundType::OPEN,
      eClosed = (int)sw::redis::BoundType::CLOSED
    };
protected:
  SortedSetWrapper(RedisConn& c) : conn(c) {}
    
  class ModifyWrapper
  {
    friend class SortedSetWrapper;
    SortedSetWrapper& wrapper;
    ModifyWrapper(SortedSetWrapper& w) : wrapper(w) {}

  public:
    
    bool ZAdd(const std::string_view key, const std::string_view member,
              double score)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> bool
          { return handle.zadd(key, member, score) > 0; });
    }

    template <typename Input>
    size_t ZAdd(const std::string_view key, Input first, Input last)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.zadd(key, first, last); });
    }

    bool ZRem(const std::string_view key, const std::string_view member)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> bool
                                    { return handle.zrem(key, member) > 0; });
    }

    template <typename T>
    size_t ZRem(const std::string_view key, T first, T last)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.zrem(key, first, last); });
    }

    template <typename T>
    size_t ZRem(const std::string_view key, std::initializer_list<T> members)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.zrem(key, members); });
    }

    double ZIncrBy(const std::string_view key, double increment,
                   const std::string_view member)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> double
          { return handle.zincrby(key, increment, member); });
    }
  };

  class QueryWrapper
  {
    friend class SortedSetWrapper;
    SortedSetWrapper& wrapper;
    QueryWrapper(SortedSetWrapper& w) : wrapper(w) {}

  public:
    size_t ZCard(const std::string_view key)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.zcard(key); });
    }

    std::optional<long long> ZRank(const std::string_view key,
                                   const std::string_view member)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> std::optional<long long>
          { return handle.zrank(key, member); });
    }

    std::optional<long long> ZRevRank(const std::string_view key,
                                      const std::string_view member)
    {
      return wrapper.conn.RedisFunc(
          [&](auto&& handle) -> std::optional<long long>
          { return handle.zrevrank(key, member); });
    }

    std::optional<double> ZScore(const std::string_view key,
                                 const std::string_view member)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> std::optional<double>
                                    { return handle.zscore(key, member); });
    }
    template <typename Interval>
    size_t ZCount(const std::string_view key, Interval interval)
    {
      return wrapper.conn.RedisFunc([&](auto&& handle) -> size_t
                                    { return handle.zcount(key, interval); });
    }

    template <typename Output>
    void ZRange(const std::string_view key, long long start, long long stop,
                Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          { handle.zrange(key, start, stop, std::forward<Output>(output)); });
    }

    template <typename Output>
    void ZRevRange(const std::string_view key, long long start, long long stop,
                   Output output)
    {
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          {
            handle.zrevrange(key, start, stop, std::forward<Output>(output));
          });
    }
 
    template <typename Output>
    void ZRangeByScore(const std::string_view key, BoundType bound, double min,
                       double max, Output output)
    {
      using Interval = sw::redis::BoundedInterval<double>;
      Interval interval(min, max, static_cast<sw::redis::BoundType>(bound));
      wrapper.conn.RedisFunc(
          [&](auto&& handle) -> void
          {
            handle.zrangebyscore(key, interval, std::forward<Output>(output));
          });
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

} // namespace AtlasNet::Database::Redis
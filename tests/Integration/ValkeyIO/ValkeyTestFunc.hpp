#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "atlasnet/core/database/redis/Redis.hpp"

using namespace AtlasNet;
using namespace AtlasNet::Database;

namespace
{
std::string ClusterSafeKey(std::string_view group, std::string_view suffix)
{
  return std::string("{") + std::string(group) + "}:" + std::string(suffix);
}

template <typename Fixture>
std::unique_ptr<RedisConn> MakeStandaloneConn(Fixture* self)
{
  RedisConn::Settings settings;
  settings.Mode = RedisConn::RedisMode::eStandalone;
  settings.host = AtlasNet::HostAddress(self->Host);
  settings.port = self->Port;
  return RedisConn::Connect(settings);
}

template <typename Fixture>
std::unique_ptr<RedisConn> MakeClusterConn(Fixture* self)
{
  RedisConn::Settings settings;
  settings.Mode = RedisConn::RedisMode::eCluster;
  settings.host = AtlasNet::HostAddress(self->Host);
  settings.port = self->Port;
  return RedisConn::Connect(settings);
}

void ExpectContainsExactly(std::vector<std::string> actual,
                           std::vector<std::string> expected)
{
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(actual, expected);
}

void RunPingLikeTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("ping", "value");

  conn.KeyVal().Delete().Del(key);

  EXPECT_TRUE(conn.KeyVal().GetSet().Set(key, "pong"));
  auto value = conn.KeyVal().GetSet().Get(key);

  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "pong");
}

void RunKeyValBasicTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("kv", "basic");
  auto renamed = ClusterSafeKey("kv", "renamed");

  conn.KeyVal().Delete().Del({key, renamed});

  EXPECT_TRUE(conn.KeyVal().GetSet().Set(key, "hello"));
  EXPECT_TRUE(conn.KeyVal().Exists().Exists(key));

  auto value = conn.KeyVal().GetSet().Get(key);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "hello");

  EXPECT_TRUE(conn.KeyVal().GetSet().Append(key, " world"));

  value = conn.KeyVal().GetSet().Get(key);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "hello world");

  conn.KeyVal().GetSet().Rename(key, renamed);
  EXPECT_FALSE(conn.KeyVal().Exists().Exists(key));
  EXPECT_TRUE(conn.KeyVal().Exists().Exists(renamed));

  value = conn.KeyVal().GetSet().Get(renamed);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "hello world");

  EXPECT_TRUE(conn.KeyVal().Delete().Del(renamed));
  EXPECT_FALSE(conn.KeyVal().Exists().Exists(renamed));
}

void RunKeyValCounterTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("kv", "counter");
  conn.KeyVal().Delete().Del(key);

  EXPECT_TRUE(conn.KeyVal().GetSet().Set(key, "10"));
  EXPECT_EQ(conn.KeyVal().GetSet().Incr(key), 11u);
  EXPECT_EQ(conn.KeyVal().GetSet().IncrBy(key, 5), 16u);
  EXPECT_EQ(conn.KeyVal().GetSet().Decr(key), 15u);
  EXPECT_EQ(conn.KeyVal().GetSet().DecrBy(key, 3), 12u);

  auto value = conn.KeyVal().GetSet().Get(key);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "12");
}

void RunHashMapTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("hash", "user");
  conn.KeyVal().Delete().Del(key);

  EXPECT_TRUE(conn.HashMap().GetSet().HSet(key, "name", "camila"));
  EXPECT_TRUE(conn.HashMap().GetSet().HSet(key, "role", "student"));
  EXPECT_TRUE(conn.HashMap().GetSet().HSet(key, "visits", "1"));

  EXPECT_TRUE(conn.HashMap().Exists().HExists(key, "name"));
  EXPECT_FALSE(conn.HashMap().Exists().HExists(key, "missing"));

  auto name = conn.HashMap().GetSet().HGet(key, "name");
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "camila");

  EXPECT_EQ(conn.HashMap().GetSet().HLen(key), 3u);
  EXPECT_EQ(conn.HashMap().GetSet().HStrLen(key, "role"), 7u);

  EXPECT_EQ(conn.HashMap().GetSet().HIncrBy(key, "visits", 4), 5);

  auto visits = conn.HashMap().GetSet().HGet(key, "visits");
  ASSERT_TRUE(visits.has_value());
  EXPECT_EQ(*visits, "5");

  std::vector<std::string> keys;
  conn.HashMap().GetSet().HKeys(key, std::back_inserter(keys));
  ExpectContainsExactly(keys, {"name", "role", "visits"});

  std::vector<std::string> vals;
  conn.HashMap().GetSet().HVals(key, std::back_inserter(vals));
  ExpectContainsExactly(vals, {"camila", "student", "5"});

  std::vector<std::pair<std::string, std::string>> all;
  conn.HashMap().GetSet().HGetAll(key, std::back_inserter(all));
  std::sort(all.begin(), all.end());
  EXPECT_EQ(all.size(), 3u);

  std::vector<std::string> fields = {"name", "role", "missing"};
  std::vector<std::optional<std::string>> out;
  conn.HashMap().GetSet().HMGet(key, fields.begin(), fields.end(),
                                std::back_inserter(out));

  ASSERT_EQ(out.size(), 3u);
  ASSERT_TRUE(out[0].has_value());
  ASSERT_TRUE(out[1].has_value());
  EXPECT_FALSE(out[2].has_value());
  EXPECT_EQ(*out[0], "camila");
  EXPECT_EQ(*out[1], "student");

  EXPECT_TRUE(conn.HashMap().Delete().HDel(key, "role"));
  EXPECT_FALSE(conn.HashMap().Exists().HExists(key, "role"));
}

void RunSetTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("set", "members");
  conn.KeyVal().Delete().Del(key);

  EXPECT_TRUE(conn.Set().Modify().SAdd(key, "alice"));
  EXPECT_EQ(conn.Set().Modify().SAdd(key, {"bob", "charlie"}), 2u);

  EXPECT_TRUE(conn.Set().Query().SIsMember(key, "alice"));
  EXPECT_FALSE(conn.Set().Query().SIsMember(key, "david"));
  EXPECT_EQ(conn.Set().Query().SCard(key), 3u);

  std::vector<std::string> members;
  conn.Set().Query().SMembers(key, std::back_inserter(members));
  ExpectContainsExactly(members, {"alice", "bob", "charlie"});

  EXPECT_TRUE(conn.Set().Modify().SRem(key, "bob"));
  EXPECT_FALSE(conn.Set().Query().SIsMember(key, "bob"));
  EXPECT_EQ(conn.Set().Query().SCard(key), 2u);

  auto popped = conn.Set().Modify().SPop(key);
  ASSERT_TRUE(popped.has_value());
  EXPECT_EQ(conn.Set().Query().SCard(key), 1u);
}

void RunSortedSetTest(RedisConn& conn)
{
  auto key = ClusterSafeKey("zset", "scores");
  conn.KeyVal().Delete().Del(key);

  EXPECT_TRUE(conn.SortedSet().Modify().ZAdd(key, "alice", 10.0));
  EXPECT_TRUE(conn.SortedSet().Modify().ZAdd(key, "bob", 20.0));
  EXPECT_TRUE(conn.SortedSet().Modify().ZAdd(key, "charlie", 15.0));

  EXPECT_EQ(conn.SortedSet().Query().ZCard(key), 3u);

  auto score = conn.SortedSet().Query().ZScore(key, "charlie");
  ASSERT_TRUE(score.has_value());
  EXPECT_DOUBLE_EQ(*score, 15.0);

  auto rank = conn.SortedSet().Query().ZRank(key, "alice");
  ASSERT_TRUE(rank.has_value());
  EXPECT_EQ(*rank, 0);

  auto revRank = conn.SortedSet().Query().ZRevRank(key, "alice");
  ASSERT_TRUE(revRank.has_value());
  EXPECT_EQ(*revRank, 2);

  EXPECT_DOUBLE_EQ(conn.SortedSet().Modify().ZIncrBy(key, 7.0, "alice"), 17.0);

  score = conn.SortedSet().Query().ZScore(key, "alice");
  ASSERT_TRUE(score.has_value());
  EXPECT_DOUBLE_EQ(*score, 17.0);

  std::vector<std::string> range;
  conn.SortedSet().Query().ZRange(key, 0, -1, std::back_inserter(range));
  ExpectContainsExactly(range, {"alice", "bob", "charlie"});

  std::vector<std::string> byScore;
  conn.SortedSet().Query().ZRangeByScore(
      key,
      AtlasNet::Database::Redis::SortedSetWrapper::BoundType::
          eClosed,
      16.0, 25.0, std::back_inserter(byScore));
  ExpectContainsExactly(byScore, {"alice", "bob"});

  EXPECT_TRUE(conn.SortedSet().Modify().ZRem(key, "bob"));
  EXPECT_EQ(conn.SortedSet().Query().ZCard(key), 2u);
}
} // namespace
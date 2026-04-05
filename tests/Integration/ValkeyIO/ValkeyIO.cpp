#include <gtest/gtest.h>

#include "ValkeyContainer.hpp"
#include "ValkeyTestFunc.hpp"
using namespace AtlasNet::Database;
using ValkeyStandaloneTest = ValkeyStandalone;
using ValkeyClusterTest = ValkeyCluster<3>;
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
TEST_F(ValkeyStandaloneTest, Ping)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunPingLikeTest(*redisConn);
}

TEST_F(ValkeyClusterTest, Ping)
{
    std::cerr << "Running cluster ping test at " << Host << ":" << Port << std::endl;
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunPingLikeTest(*redisConn);
}

TEST_F(ValkeyStandaloneTest, KeyVal_Basic)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunKeyValBasicTest(*redisConn);
}

TEST_F(ValkeyClusterTest, KeyVal_Basic)
{
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunKeyValBasicTest(*redisConn);
}

TEST_F(ValkeyStandaloneTest, KeyVal_CounterOps)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunKeyValCounterTest(*redisConn);
}

TEST_F(ValkeyClusterTest, KeyVal_CounterOps)
{
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunKeyValCounterTest(*redisConn);
}

TEST_F(ValkeyStandaloneTest, HashMap_Basic)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunHashMapTest(*redisConn);
}

TEST_F(ValkeyClusterTest, HashMap_Basic)
{
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunHashMapTest(*redisConn);
}

TEST_F(ValkeyStandaloneTest, Set_Basic)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunSetTest(*redisConn);
}

TEST_F(ValkeyClusterTest, Set_Basic)
{
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunSetTest(*redisConn);
}

TEST_F(ValkeyStandaloneTest, SortedSet_Basic)
{
  auto redisConn = MakeStandaloneConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunSortedSetTest(*redisConn);
}

TEST_F(ValkeyClusterTest, SortedSet_Basic)
{
  auto redisConn = MakeClusterConn(this);
  ASSERT_NE(redisConn, nullptr);
  RunSortedSetTest(*redisConn);
}
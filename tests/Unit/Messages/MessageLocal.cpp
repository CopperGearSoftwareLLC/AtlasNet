#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/EndPoint.hpp"

#include "atlasnet/core/System.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <mutex>

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
TEST(MessageLocal, InitAndShutdown)
{
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();

  AtlasNet::ISystem::ShutdownAll();

  SUCCEED();
}
TEST(MessageLocal, OpenListenSocket)
{
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();

  const PortType port = 8080;
  AtlasNet::MessageSystem::Get().OpenListenSocket(port);

  SUCCEED();

  AtlasNet::ISystem::ShutdownAll();
}
ATLASNET_MESSAGE(IOTestMessage, ATLASNET_MESSAGE_DATA(int, intVal),
                 ATLASNET_MESSAGE_DATA(std::string, strVal))
TEST(MessageLocal, MessageIO)
{
  IOTestMessage msg;
  msg.intVal = 123;
  msg.strVal = "Hello, AtlasNet!";
  AtlasNet::ByteWriter writer;
  msg.Serialize(writer);
  auto bytes = writer.bytes();

  AtlasNet::ByteReader readerID(bytes);
  AtlasNet::MessageIDHash typeHash =
      AtlasNet::IMessage::DeserializeTypeIdHash(readerID);
  EXPECT_EQ(typeHash, IOTestMessage::TypeIdHash);
  AtlasNet::ByteReader reader(bytes);
  IOTestMessage deserializedMsg;
  deserializedMsg.Deserialize(reader);
  EXPECT_EQ(deserializedMsg.intVal, msg.intVal);
  EXPECT_EQ(deserializedMsg.strVal, msg.strVal);
}
TEST(MessageLocal, Connect)
{
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();

  const PortType port = 8080;
  const DNSAddress dnsAddr("localhost");
  EndPointAddress serverAddr(dnsAddr, port);
  AtlasNet::MessageSystem::Get().OpenListenSocket(port);
  AtlasNet::MessageSystem::Get().Connect(serverAddr);

  float timeout = 5.0f; // seconds
  auto startTime = std::chrono::steady_clock::now();

  while (AtlasNet::MessageSystem::Get().GetNumConnections() == 0)
  {
    std::optional<AtlasNet::MessageSystem::Connection> conn =
        AtlasNet::MessageSystem::Get().GetConnection(serverAddr);
    if (conn.has_value() &&
        conn->GetState() == AtlasNet::ConnectionState::eConnected)
    {
      SUCCEED() << "Successfully connected to localhost:" << port;
      break;
    }
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    if (elapsed > std::chrono::duration<float>(timeout))
    {
      FAIL() << "Connection timed out after " << timeout << " seconds";
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  AtlasNet::ISystem::ShutdownAll();
}
ATLASNET_MESSAGE(TestMessage, ATLASNET_MESSAGE_DATA(int, u8_val),
                 ATLASNET_MESSAGE_DATA(std::string, str))
TEST(MessageLocal, SendMessage)
{
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();
  const PortType port = 8080;
  const DNSAddress dnsAddr("localhost");
  EndPointAddress serverAddr(dnsAddr, port);
  AtlasNet::MessageSystem::Get().OpenListenSocket(port);
  AtlasNet::MessageSystem::Get().Connect(serverAddr);
  AtlasNet::MessageSystem::Get().On<TestMessage>(
      [&](const TestMessage& msg, const EndPointAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  AtlasNet::MessageSystem::Get().SendMessage(
      msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
  AtlasNet::ISystem::ShutdownAll();
};

TEST(MessageLocal, SendMessageWithoutConnecting)
{
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();
  const PortType port = 8080;
  const DNSAddress dnsAddr("localhost");
  EndPointAddress serverAddr(dnsAddr, port);
  AtlasNet::MessageSystem::Get().OpenListenSocket(port);
  AtlasNet::MessageSystem::Get().On<TestMessage>(
      [&](const TestMessage& msg, const EndPointAddress& address)
      {
        EXPECT_EQ(msg.u8_val, 42);
        EXPECT_EQ(msg.str, "Hello, AtlasNet!");
        SUCCEED() << "Received message from " << address.to_string();
        std::lock_guard lock(mtx);
        cv.notify_one();
        success = true;
      });
  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  AtlasNet::MessageSystem::Get().SendMessage(
      msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
  AtlasNet::ISystem::ShutdownAll();
};
TEST(MessageLocal, ListenMessagePort)
{
  using namespace AtlasNet;

  std::atomic_int PortCount = 0;
  std::atomic_int AllCount = 0;
  std::mutex mtx;
  std::condition_variable cv;
  JobSystem::Init();
  MessageSystem::Init();
  const PortType port1 = 8080, port2 = 8081;

  EndPointAddress serverAddr(DNSAddress("localhost"), port1);
  EndPointAddress serverAddr2(DNSAddress("localhost"), port2);

  MessageSystem::Get().OpenListenSocket(port2);
  MessageSystem::Get().OpenListenSocket(port1).On<TestMessage>(
      [&](const TestMessage& msg, const EndPointAddress& address)
      { PortCount++; });

  MessageSystem::Get().On<TestMessage>(
      [&](const TestMessage& msg, const EndPointAddress& address)
      {
        AllCount++;
        std::lock_guard lock(mtx);
        cv.notify_one();
      });

  MessageSystem::Get().Connect(serverAddr);

  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  MessageSystem::Get().SendMessage(msg, serverAddr,
                                   AtlasNet::MessageSendMode::eReliable);
  MessageSystem::Get().SendMessage(msg, serverAddr2,
                                   AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] { return AllCount.load() >= 2; }))
      << "Did not receive messages within timeout";
  EXPECT_EQ(PortCount.load(), 1) << "Port-specific handler should have been called once";
  EXPECT_GE(AllCount.load(), 2) << "General handler should have been called for all messages";
}
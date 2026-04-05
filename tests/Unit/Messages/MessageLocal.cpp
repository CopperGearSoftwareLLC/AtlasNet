#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"

#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/serialize/ByteWriter.hpp"
#include "atlasnet/core/system/isystem.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <mutex>
#include <netinet/in.h>

int pick_available_port()
{
  int Min = 1024;
  int Max = 65535;
  if (Min > Max)
    std::swap(Min, Max);

  auto can_bind = [](int port, int sock_type) -> bool
  {
    int fd = ::socket(AF_INET, sock_type, 0);
    if (fd < 0)
      return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    const bool ok =
        (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    ::close(fd);
    return ok;
  };

  for (int port = Min; port <= Max; ++port)
  {
    // Consider the port "available" only if both TCP and UDP can bind.
    if (can_bind(port, SOCK_STREAM) && can_bind(port, SOCK_DGRAM))
      return port;
  }

  return -1; // no free port in range
}
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
TEST(MessageLocal, InitAndShutdown)
{
  using namespace AtlasNet;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});

  SUCCEED();
}
TEST(MessageLocal, OpenListenSocket)
{
  using namespace AtlasNet;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});

  const PortType port = pick_available_port();
  msgsys.OpenListenSocket(port);

  SUCCEED();
}
ATLASNET_MESSAGE(IOTestMessage, ATLASNET_MESSAGE_DATA(int, intVal),
                 ATLASNET_MESSAGE_DATA(std::string, strVal))
TEST(MessageLocal, MessageIO)
{
  using namespace AtlasNet;
  IOTestMessage msg;
  msg.intVal = 123;
  msg.strVal = "Hello, AtlasNet!";
  ByteWriter writer;
  msg.Serialize(writer);
  auto bytes = writer.bytes();

  ByteReader readerID(bytes);
  MessageIDHash typeHash = IMessage::DeserializeTypeIdHash(readerID);
  EXPECT_EQ(typeHash, IOTestMessage::TypeIdHash);
  ByteReader reader(bytes);
  IOTestMessage deserializedMsg;
  deserializedMsg.Deserialize(reader);
  EXPECT_EQ(deserializedMsg.intVal, msg.intVal);
  EXPECT_EQ(deserializedMsg.strVal, msg.strVal);
}
TEST(MessageLocal, Connect)
{
  using namespace AtlasNet;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});

  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.Connect(serverAddr);

  float timeout = 5.0f; // seconds
  auto startTime = std::chrono::steady_clock::now();

  while (msgsys.GetNumConnections() == 0)
  {
    std::optional<MessageSystem::Connection> conn =
        msgsys.GetConnection(serverAddr);
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
}
ATLASNET_MESSAGE(TestMessage, ATLASNET_MESSAGE_DATA(int, u8_val),
                 ATLASNET_MESSAGE_DATA(std::string, str))
TEST(MessageLocal, SendMessage)
{
  using namespace AtlasNet;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.Connect(serverAddr);
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
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
  msgsys.SendMessage(msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
};

TEST(MessageLocal, SendMessageWithoutConnecting)
{
  bool success = false;
  std::mutex mtx;
  std::condition_variable cv;
  using namespace AtlasNet;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});
  const PortType port = pick_available_port();
  const HostName dnsAddr("localhost");
  SocketAddress serverAddr(dnsAddr, port);
  msgsys.OpenListenSocket(port);
  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
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
  msgsys.SendMessage(msg, serverAddr, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(
      cv.wait_for(lock, std::chrono::seconds(5), [&] { return success; }))
      << "Did not receive message within timeout";

  EXPECT_TRUE(success) << "Did not receive message within timeout";
};
TEST(MessageLocal, ListenMessagePort)
{
  using namespace AtlasNet;

  std::atomic_int PortCount = 0;
  std::atomic_int AllCount = 0;
  std::mutex mtx;
  std::condition_variable cv;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});
  const PortType port1 = pick_available_port();
  SocketAddress serverAddr(HostName("localhost"), port1);
  msgsys.OpenListenSocket(port1);

  const PortType port2 = pick_available_port();
  EXPECT_NE(port1, port2);
  SocketAddress serverAddr2(HostName("localhost"), port2);
  msgsys.OpenListenSocket(port2).On<TestMessage>(
    [&](const TestMessage& msg, const SocketAddress& address)
    {
      PortCount++;
      std::lock_guard lock(mtx);
      cv.notify_one();
    });

  msgsys.On<TestMessage>(
      [&](const TestMessage& msg, const SocketAddress& address)
      {
        AllCount++;
        std::lock_guard lock(mtx);
        cv.notify_one();
      });

  msgsys.Connect(serverAddr);

  TestMessage msg;
  msg.u8_val = 42;
  msg.str = "Hello, AtlasNet!";
  msgsys.SendMessage(msg, serverAddr, AtlasNet::MessageSendMode::eReliable);
  msgsys.SendMessage(msg, serverAddr2, AtlasNet::MessageSendMode::eReliable);

  std::unique_lock lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
                        [&]
                        {
                          return AllCount.load() >= 2 &&
                                 PortCount.load() >= 1;
                        }))
    << "Did not receive messages within timeout";
  EXPECT_EQ(PortCount.load(), 1)
      << "Port-specific handler should have been called once";
  EXPECT_GE(AllCount.load(), 2)
      << "General handler should have been called for all messages";
}
ATLASNET_MESSAGE(BigMessageTestMessage,
                 ATLASNET_MESSAGE_DATA(std::vector<uint8_t>, data))

TEST(MessageLocal, BigMessage)
{
  using namespace AtlasNet;
  using namespace std::chrono_literals;
  JobSystem jobsys(JobSystem::Config{});
  MessageSystem msgsys(MessageSystem::Config{.jobSystem = &jobsys});

  std::vector<std::size_t> sizes = {
      1ull,     10ull,     100ull,
      1'000ull, 10'000ull, 100'000ull, // 1'000'000ull, 10'000'000ull,
                                       // 100'000'000ull,
                                       // 1'000'000'000ull
  };

  std::mutex mtx;
  std::condition_variable cv;
  std::vector<uint8_t> receivedData;

  const PortType port = pick_available_port();
  SocketAddress addr("127.0.0.1:" + std::to_string(port));

  msgsys.OpenListenSocket(port);

  msgsys.On<BigMessageTestMessage>(
      [&](const BigMessageTestMessage& msg, const SocketAddress&)
      {
        {
          std::lock_guard lock(mtx);
          receivedData = msg.data;
        }
        cv.notify_all();
      });

  auto conn = msgsys.Connect(addr);
  ASSERT_TRUE(conn.valid());

  for (std::size_t currentSize : sizes)
  {
    std::vector<uint8_t> bigData(currentSize);
    for (std::size_t j = 0; j < currentSize; ++j)
    {
      bigData[j] = static_cast<uint8_t>(j % 256);
    }

    {
      std::lock_guard lock(mtx);
      receivedData.clear();
    }

    BigMessageTestMessage msg;
    msg.data = bigData;

    msgsys.SendMessage(msg, addr, MessageSendMode::eReliable);

    {
      std::unique_lock lock(mtx);
      ASSERT_TRUE(
          cv.wait_for(lock, 30s, [&] { return receivedData == bigData; }))
          << "Timed out waiting for correct message of size " << currentSize
          << ", last received size was " << receivedData.size();
    }

    ASSERT_EQ(receivedData.size(), bigData.size())
        << "Size mismatch for payload size " << currentSize;

    ASSERT_EQ(receivedData, bigData)
        << "Content mismatch for payload size " << currentSize;
  }
}
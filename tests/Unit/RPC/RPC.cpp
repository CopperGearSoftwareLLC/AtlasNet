#pragma once
#include "atlasnet/core/RPC/RPC.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include "atlasnet/core/RPC/RPCMessage.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"

#include <condition_variable>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace AtlasNet;
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

ATLASNET_RPC(TESTRpc, ATLASNET_RPC_METHOD(TestMethod, void, int, float);
             ATLASNET_RPC_METHOD(TestMethod_Ret, int);
             ATLASNET_RPC_METHOD(TestMethod_Ret_String, std::string,
                                 std::string_view););
TEST(RPC, BaseMessage)
{
  JobSystem jobsystem(JobSystem::Config{});
  MessageSystem msgSystem(
      MessageSystem::Config{.jobSystem = &jobsystem});
  const PortType port = 41001;
  std::mutex mutex;
  bool success = false;
  std::condition_variable cv;
  std::cout << std::format("Request Hash ID: {}", RpcRequestMessage::TypeIdHash)
            << std::endl;
  std::cout << std::format("Response Hash ID: {}",
                           RpcResponseMessage::TypeIdHash)
            << std::endl;
  msgSystem.OpenListenSocket(port)
      .On<RpcRequestMessage>(
          [&](const RpcRequestMessage& msg, const SocketAddress&)
          {
            RpcResponseMessage response{
                .callID = msg.callID,
                .payload = std::vector<uint8_t>{1, 2, 3, 4, 5},
            };
            msgSystem.SendMessage(
                response, SocketAddress(IPv4(127, 0, 0, 1), port),
                MessageSendMode::eReliableBatched);
          })
      .On<RpcResponseMessage>(
          [&](const RpcResponseMessage& msg, const SocketAddress&)
          {
            success = true;
            cv.notify_one();
          });

  RpcRequestMessage request{
      .methodId = 123,
      .callID = 456,
      .payload = std::vector<uint8_t>{10, 20, 30},
  };
  msgSystem.SendMessage(request,
                        SocketAddress(IPv4(127, 0, 0, 1), port),
                        MessageSendMode::eReliableBatched);
  std::unique_lock lock(mutex);
  cv.wait_for(lock, std::chrono::seconds(5), [&success] { return success; });
  EXPECT_TRUE(success);
}
TEST(RPC, SelfReceive)
{
  JobSystem jobSystem(JobSystem::Config{});
  const PortType port = 41001;

  MessageSystem msgSystem(
      MessageSystem::Config{.jobSystem = &jobSystem});
  RPC rpc(
      RPC::Config{.port = port, .messageSystem = &msgSystem});

  bool success = false;
  std::mutex mutex;
  std::condition_variable cv;
  rpc.Bind<TESTRpc::TestMethod>(
      [&](int a, float b)
      {
        std::cout << "TestMethod called with a=" << a << " b=" << b
                  << std::endl;
        success = true;
        cv.notify_one();
      });
  rpc.Call<TESTRpc::TestMethod>(
      SocketAddress(IPv4(127, 0, 0, 1), port), 42, 3.14f);

  std::unique_lock lock(mutex);
  cv.wait_for(lock, std::chrono::seconds(5), [&success] { return success; });
  EXPECT_TRUE(success);
}
TEST(RPC, SelfReceiveAndReply)
{
  JobSystem jobSystem(JobSystem::Config{});

  MessageSystem msgSystem(
      MessageSystem::Config{.jobSystem = &jobSystem});
  const PortType port = 41001;
  RPC rpc(
      RPC::Config{.port = port, .messageSystem = &msgSystem});
  bool success = false;

  rpc.Bind<TESTRpc::TestMethod_Ret_String>(
      [&](std::string_view str) -> std::string
      {
        std::cout << "TestMethod_Ret_String called with str=" << str
                  << std::endl;
        success = true;

        std::string strCopy(str);
        return strCopy + " world";
      });
  std::cout << std::format("Request Hash ID: {}", RpcRequestMessage::TypeIdHash)
            << std::endl;
  std::cout << std::format("Response Hash ID: {}",
                           RpcResponseMessage::TypeIdHash)
            << std::endl;
  std::future<std::string> result = rpc.Call<TESTRpc::TestMethod_Ret_String>(
      SocketAddress(IPv4(127, 0, 0, 1), port), "Hello");

  auto status = result.wait_for(std::chrono::seconds(5));
  if (status == std::future_status::ready)
  {
    std::string resultValue = result.get();
    EXPECT_EQ(resultValue, "Hello world");
  }
  else
  {
    FAIL() << "Future did not become ready in time";
  }

  EXPECT_TRUE(success);
}
TEST(RPC, SelfReceiveWrongPort)
{
  JobSystem jobSystem(JobSystem::Config{});
  const PortType port = 41001;

  MessageSystem msgSystem(
      MessageSystem::Config{.jobSystem = &jobSystem});
  RPC rpc(
      RPC::Config{.port = port, .messageSystem = &msgSystem});

  bool success = false;
  std::mutex mutex;
  std::condition_variable cv;
  rpc.Bind<TESTRpc::TestMethod>(
      [&](int a, float b)
      {
        std::cout << "TestMethod called with a=" << a << " b=" << b
                  << std::endl;
        success = true;
        cv.notify_one();
      });
  rpc.Call<TESTRpc::TestMethod>(
      SocketAddress(IPv4(127, 0, 0, 1), port+1), 42, 3.14f);

  std::unique_lock lock(mutex);
  cv.wait_for(lock, std::chrono::seconds(5), [&success] { return success; });
  EXPECT_FALSE(success);
}

TEST(RPC, ForkParentCallsChildAndGetsResult)
{
  const PortType parentPort = 41011;
  const PortType childPort = 41012;

  int readyPipe[2];
  ASSERT_EQ(pipe(readyPipe), 0) << "Failed to create pipe";

  const int expectedResult = std::chrono::system_clock::now().time_since_epoch().count() % 10000; // Just some arbitrary value to return from child to parent
  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork() failed";
  if (pid == 0)
  {
    // Child process: hosts RPC server on childPort.
    close(readyPipe[0]);

    JobSystem childJobSystem(JobSystem::Config{});
    MessageSystem childMsgSystem(
        MessageSystem::Config{.jobSystem = &childJobSystem});
    RPC childRpc(
        RPC::Config{.port = childPort, .messageSystem = &childMsgSystem});

    std::mutex mutex;
    std::condition_variable cv;
    bool handled = false;

    childRpc.Bind<TESTRpc::TestMethod_Ret>(
        [&]() -> int
        {
          {
            std::lock_guard<std::mutex> lock(mutex);
            handled = true;
            std::cerr << "Child received TestMethod_Ret call" << std::endl;
            
          }
          cv.notify_one();
          return expectedResult;
        });

    // Signal readiness to parent.
    const uint8_t ready = 1;
    (void)write(readyPipe[1], &ready, 1);
    close(readyPipe[1]);

    std::unique_lock<std::mutex> lock(mutex);
    const bool gotCall =
        cv.wait_for(lock, std::chrono::seconds(10), [&] { return handled; });
       std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cerr << "Exiting child process" << std::endl;
    _exit(gotCall ? 0 : 2);
  }

  // Parent process: sets up its own RPC on parentPort, then calls child.
  close(readyPipe[1]);

  uint8_t ready = 0;
  ASSERT_EQ(read(readyPipe[0], &ready, 1), 1) << "Parent failed waiting for child readiness";
  close(readyPipe[0]);

  JobSystem parentJobSystem(JobSystem::Config{});
  MessageSystem parentMsgSystem(
      MessageSystem::Config{.jobSystem = &parentJobSystem});
  RPC parentRpc(
      RPC::Config{.port = parentPort, .messageSystem = &parentMsgSystem});

  std::future<int> result = parentRpc.Call<TESTRpc::TestMethod_Ret>(
      SocketAddress(IPv4(127, 0, 0, 1), childPort));

  auto status = result.wait_for(std::chrono::seconds(10));
  ASSERT_EQ(status, std::future_status::ready) << "RPC future not ready in time";
  EXPECT_EQ(result.get(), expectedResult);

  int childStatus = 0;
  ASSERT_EQ(waitpid(pid, &childStatus, 0), pid);
  ASSERT_TRUE(WIFEXITED(childStatus));
  EXPECT_EQ(WEXITSTATUS(childStatus), 0);
}


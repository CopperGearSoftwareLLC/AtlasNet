

#include "TestUtils.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/Message.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include "atlasnet/core/system/isystem.hpp"
#include <chrono>
#include <format>
#include <future>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <unistd.h>

struct CommandResult
{
  int exitCode = -1;
  std::string output;
};

CommandResult RunCommand(const std::string& cmd)
{
  CommandResult result;
  std::array<char, 256> buffer{};

  std::string fullCmd = cmd + " 2>&1";
  FILE* pipe = popen(fullCmd.c_str(), "r");
  if (!pipe)
  {
    throw std::runtime_error("popen failed");
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
  {
    result.output += buffer.data();
  }

  int status = pclose(pipe);
  if (WIFEXITED(status))
  {
    result.exitCode = WEXITSTATUS(status);
  }
  else
  {
    result.exitCode = -1;
  }

  return result;
}
TEST(MESSAGES, SelfConnect)
{
  PortType port = 12345;
  EndPointAddress localhost("127.0.0.1:12345");

  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init().OpenListenSocket(port);
  AtlasNet::MessageSystem::Get().Connect(localhost);
  bool success = false;
  int tries = 15;
  while (true)
  {
    if (tries-- <= 0)
    {
      std::cerr << "Did not receive a connection after multiple attempts"
                << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (AtlasNet::MessageSystem::Get().GetNumConnections() > 0)
    {
      success = true;
      break;
    }
  }
  std::cerr << std::format("Successfully connected to server") << std::endl;
  EXPECT_TRUE(success);
}
TEST(MESSAGES, Connect)
{
  int test_num = 0;
  std::string serverCmd =
      std::format("{} --test-num {}", SERVER_BINARY, test_num);
  std::string clientCmd =
      std::format("{} --test-num {} ", CLIENT_BINARY, test_num);

  std::promise<CommandResult> serverPromise, clientPromise;

  std::thread serverThread(
      [&]()
      {
        CommandResult serverResult = RunCommand(serverCmd);
        serverResult.output = PrefixLines(serverResult.output, "Server");
        serverPromise.set_value(std::move(serverResult));
      });

  std::thread clientThread(
      [&]()
      {
        CommandResult clientResult = RunCommand(clientCmd);
        clientResult.output = PrefixLines(clientResult.output, "Client");
        clientPromise.set_value(std::move(clientResult));
      });

  CommandResult serverResult = serverPromise.get_future().get();
  CommandResult clientResult = clientPromise.get_future().get();

  serverThread.join();
  clientThread.join();
  std::cerr << serverResult.output;
  std::cerr << clientResult.output;
  EXPECT_EQ(serverResult.exitCode, 0);
  EXPECT_EQ(clientResult.exitCode, 0);
}

TEST(MESSAGES, SingleMessage)
{
  int test_num = 1;
  std::string serverCmd =
      std::format("{} --test-num {}", SERVER_BINARY, test_num);
  std::string clientCmd =
      std::format("{} --test-num {} ", CLIENT_BINARY, test_num);

  std::promise<CommandResult> serverPromise, clientPromise;

  std::thread serverThread(
      [&]()
      {
        CommandResult serverResult = RunCommand(serverCmd);
        serverResult.output = PrefixLines(serverResult.output, "Server");
        serverPromise.set_value(std::move(serverResult));
      });

  std::thread clientThread(
      [&]()
      {
        CommandResult clientResult = RunCommand(clientCmd);
        clientResult.output = PrefixLines(clientResult.output, "Client");
        clientPromise.set_value(std::move(clientResult));
      });

  CommandResult serverResult = serverPromise.get_future().get();
  CommandResult clientResult = clientPromise.get_future().get();

  serverThread.join();
  clientThread.join();
  std::cerr << serverResult.output;
  std::cerr << clientResult.output;
  EXPECT_EQ(serverResult.exitCode, 0);
  EXPECT_EQ(clientResult.exitCode, 0);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
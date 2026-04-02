#pragma once

#include "Messages.hpp"
#include "TestUtils.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/job/Job.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
 std::optional<EndPointAddress> serverAddr;
int test0()
{

  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();

  std::cout << "Connecting to server at " << serverAddr->to_string() << std::endl;
  AtlasNet::MessageSystem::Get().Connect(serverAddr.value());

  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 0;
}
int test1()
{


  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();
  SimpleMessage msg;
  msg.aNumber = 42;
  msg.str = "Hello from client!";
  AtlasNet::MessageSystem::Get().SendMessage(
      msg, serverAddr.value(), AtlasNet::MessageSendMode::eReliable);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 0;
}
const static inline std::unordered_map<int, std::function<int()>> tests = {
    {0, test0}, {1, test1}};
int main(int argc, char** argv)
{

  TestUtils::Init();
  // Look for --server-addr and --test-num in args

  std::cerr << "Running Client with args: ";
  for (int i = 1; i < argc; ++i)
  {
    std::cerr << argv[i] << " ";
  }
  std::cerr << std::endl;
  std::optional<int> testNum = 0;
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--test-num" && i + 1 < argc)
    {
      testNum = std::stoi(argv[++i]);
    }
    else if (arg == "--server-addr" && i + 1 < argc)
    {
      serverAddr = EndPointAddress(argv[++i]);
    }
  }

  if (!testNum)
  {
    std::cerr << std::format("Usage: {} --test-num <num>", argv[0])
              << std::endl;
    std::string args;
    for (int i = 1; i < argc; ++i)
    {
      args += argv[i];
      args += " ";
    }
    std::cerr << std::format("Received args: {}", args) << std::endl;

    int overallResult = 0;
    for (const auto& [ID, func] : tests)
    {

      int result = func();
      overallResult += result;
      std::cerr << std::format("Test {} returned {}", ID, result) << std::endl;
    }
    return overallResult;
  }

  return tests.at(*testNum)();
}

#include "Messages.hpp"
#include "TestUtils.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"
#include <condition_variable>
#include <future>
#include <mutex>
#include <unordered_map>
int test0()
{
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init().OpenListenSocket(12345);

  int tries = 15;
  while (true)
  {
    if (tries-- <= 0)
    {
      std::cerr << "Did not receive a connection after multiple attempts"
                << std::endl;
      throw std::runtime_error("Test failed: could not connect to server");
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (AtlasNet::MessageSystem::Get().GetNumConnections() > 0)
    {
      break;
    }
  }
  std::cerr << std::format("Successfully connected to server") << std::endl;

  return 0;
}
int test1()
{
  SimpleMessage msg;
  std::mutex msgMutex;
  std::condition_variable msgCv;
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init()
      .OpenListenSocket(12345)
      .On<SimpleMessage>(
          [&](const SimpleMessage& _msg)
          {
            msg = _msg;
            std::cerr << std::format("Received message: aNumber={}, str={}",
                                _msg.aNumber, _msg.str)
                        << std::endl;

            msgCv.notify_one();
          });

  std::unique_lock<std::mutex> lock(msgMutex);
  if (msgCv.wait_for(lock, std::chrono::seconds(50)) == std::cv_status::timeout)
  {
    std::cerr << std::format("Timed out waiting for message") << std::endl;
    throw std::runtime_error("Test failed: did not receive message in time");
  }

  if (msg.aNumber != 42 || msg.str != "Hello from client!")
  {
    std::cerr << std::format("Received incorrect message: aNumber={}, str={}",
                             msg.aNumber, msg.str)
              << std::endl;

    throw std::runtime_error("Test failed: received incorrect message");
  }

  std::cerr << std::format("Test passed: received correct message") << std::endl;

  return 0;
}
const static inline std::unordered_map<int, std::function<int()>> tests = {
    {0, test0}};
int main(int argc, char** argv)
{
TestUtils::Init();

  // Look for --server-addr and --test-num in args

  std::optional<int> testNum = 0;
  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--test-num" && i + 1 < argc)
    {
      testNum = std::stoi(argv[++i]);
    }
  }
  if (!testNum)
  {
    std::cerr << std::format("Usage: {} --test-num <num>", argv[0])
              << std::endl;
    std::cerr << "Running all tests...\n";
    for (const auto& [num, testFunc] : tests)
    {
      try
      {
        testFunc();
      }
      catch (const std::exception& e)
      {
        std::cerr << std::format("Test {} failed with exception: {}", num,
                                 e.what())
                  << std::endl;
      }
    }
    return 0;
  }

  return tests.at(testNum.value())();
}
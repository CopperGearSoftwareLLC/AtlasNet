#pragma once
#include "atlasnet/core/RPC/RPC.hpp"
#include "atlasnet/core/database/RedisConn.hpp"
#include "atlasnet/core/job/JobSystem.hpp"
#include "atlasnet/core/messages/MessageSystem.hpp"

#include <gtest/gtest.h>
#include <iostream>
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

ATLASNET_RPC(
    TESTRpc, ATLASNET_RPC_METHOD(TestMethod, void, int, float);
    ATLASNET_RPC_METHOD(TestMethod_Ret, int);
    ATLASNET_RPC_METHOD(TestMethod_Ret_String, std::string, std::string_view);
);


TEST(RPC, SelfTest)
{
  AtlasNet::JobSystem::Init();
  AtlasNet::MessageSystem::Init();
  const PortType port = 41001;
  AtlasNet::RPC rpc(AtlasNet::RPC::Settings{.port = port});
  bool success = false;
  rpc.Bind<TESTRpc::TestMethod>(
      [&](int a, float b)
      {
        std::cout << "TestMethod called with a=" << a << " b=" << b
                  << std::endl;
        success = true;
      }

  );

  rpc.Call<TESTRpc::TestMethod>(
      EndPointAddress(IPv4Address(127, 0, 0, 1), port), 42, 3.14f
  );

  EXPECT_TRUE(success);
}
/*   void Test()
  {
    auto test_method_bind = [](int a, float b) {};
    auto second_test_method_bind = [](const std::string_view) { return 1; };

    RPC::Bind<TESTRpc::TestMethod>(test_method_bind);
    RPC::Bind<TESTRpc::SecondTestMethod>(second_test_method_bind);

    RPCTarget target = EndPointAddress(IPv4Address(127, 0, 0, 1), 8080);
    RPCTarget target2 = ContainerID();

    RPC::Call<TESTRpc::TestMethod>(target, 42, 3.14f);
    std::future<int> second_result =
  RPC::Call<TESTRpc::SecondTestMethod>(target, "Hello");

    int result = second_result.get();
  } */

/*
TEST(RPC, BasicOps)
{
  constexpr const char* kAddr = "127.0.0.1";
  constexpr uint16_t kParentPort = 41001;
  constexpr uint16_t kChildPort = 41002;

  // Ensure ports are different.
  ASSERT_NE(kParentPort, kChildPort);

  pid_t pid = fork();
  ASSERT_NE(pid, -1) << "fork() failed";
  auto TestMethodBind = [](int a, float b)
  { std::cout << "TestMethod called with a=" << a << " b=" << b << std::endl; };
  auto TestMethodRetBind = []() -> int
  {
    std::cout << "SecondTestMethod called" << std::endl;
    return 42;
  };
  auto TestMethodRetStringBind = [](std::string_view str) -> std::string
  {
    std::cout << "TestMethod_Ret_String called with str=" << str << std::endl;
    return std::string(str) + " world";
  };

  if (pid == 0)
  {
    AtlasNet::RPC::Settings settings{.port = kChildPort};
    AtlasNet::RPC rpc(settings);

    rpc.Bind<TESTRpc::TestMethod>(TestMethodBind);
    rpc.Bind<TESTRpc::TestMethod_Ret>(TestMethodRetBind);
    rpc.Bind<TESTRpc::TestMethod_Ret_String>(TestMethodRetStringBind);



    // Child process runs the RPC server
    while (true)
    {
      // In a real implementation, you'd have a proper event loop here
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  else
  {
    AtlasNet::RPC::Settings settings{.port = kParentPort};
    AtlasNet::RPC rpc(settings);

    // Parent process runs the RPC client
    //RPCTarget target = EndPointAddress(IPv4Address(127, 0, 0, 1), kChildPort);

    rpc.Call<TESTRpc::TestMethod>(target, 42, 3.14f);
    auto second_result_future = rpc.Call<TESTRpc::TestMethod_Ret>(target);
    auto string_result_future = rpc.Call<TESTRpc::TestMethod_Ret_String>(target,
"Hello");

    int second_result = second_result_future.get();
    std::string string_result = string_result_future.get();

    std::cout << "Received from TestMethod_Ret: " << second_result << std::endl;
    std::cout << "Received from TestMethod_Ret_String: " << string_result <<
std::endl;
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);

  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
  if (pid == 0)
  {
    AtlasNet::RPC::Settings settings{.port = kChildPort};
    //AtlasNet::RPC rpc(settings);

    bool ok = true;

    _exit(ok ? 0 : 11);
  }
  else
  {
    AtlasNet::RPC::Settings settings{.port = kParentPort};
   // AtlasNet::RPC rpc(settings);

    // Parent process
  }

  ASSERT_EQ(waitpid(pid, &status, 0), pid);

  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
} */


#include "atlasnet/core/Address.hpp"
#include "atlasnet/core/SocketAddress.hpp"
#include "atlasnet/core/database/Redis.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace AtlasNet;
using namespace AtlasNet::Database;
bool KeyValTest(RedisConn& conn)
{
  for (int i = 0; i < 100; i++)
  {
    std::string ranKey, ranVal;
    for (int j = 0; j < 10; j++)
    {
      ranKey += static_cast<char>('a' + rand() % 26);
      ranVal += static_cast<char>('a' + rand() % 26);
    }

    conn.KeyVal().GetSet().Set(ranKey, ranVal);
    auto value = conn.KeyVal().GetSet().Get(ranKey);
    if (value)
    {
      std::cout << std::format("Successfully got value: {}", *value)
                << std::endl;
      if (*value == ranVal)
      {
        std::cout << std::format("Value {} is correct!", *value) << std::endl;
        continue;
      }
      else
      {
        std::cerr << std::format("Value is incorrect: {}", *value) << std::endl;
        return false;
      }
    }
    else
    {
      std::cerr << "Failed to get value for key" << std::endl;
      return false;
    }
    return false;
  }
  return true;
}
int main(int argc, char** argv)
{
  std::string host =
      std::getenv("VALKEY_HOST") ? std::getenv("VALKEY_HOST") : "INVALID";
  std::string port =
      std::getenv("VALKEY_PORT") ? std::getenv("VALKEY_PORT") : "INVALID";
  RedisConn::Settings settings;
  HostName address(host);
  const bool clusterMode =
      std::getenv("VALKEY_CLUSTER") &&
      std::string_view(std::getenv("VALKEY_CLUSTER")) == "1";

  settings.Mode = clusterMode ? RedisConn::RedisMode::eCluster
                              : RedisConn::RedisMode::eStandalone;
  std::cout << "Running in " << (clusterMode ? "Cluster" : "Standalone")
            << " mode" << std::endl;
  settings.ExceptionOnFailure = true;
  settings.MaxConnectRetries = 5;

  std::cout << "Connecting to Redis at " << host << ":" << port << std::endl;

  settings.host = address;
  settings.port = static_cast<PortType>(std::atoi(port.c_str()));

  std::unique_ptr<RedisConn> conn = RedisConn::Connect(settings);

  if (!conn)
    return 1;
  std::cout << "Successfully connected to Redis!" << std::endl;

  if (!KeyValTest(*conn))
  {
    return 1;
  }

  return 0;
}
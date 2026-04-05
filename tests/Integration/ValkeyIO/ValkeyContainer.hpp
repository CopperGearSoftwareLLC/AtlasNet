#pragma once

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

class DockerError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

inline std::string Trim(std::string s)
{
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' ||
                        s.back() == ' ' || s.back() == '\t'))
  {
    s.pop_back();
  }

  size_t start = 0;
  while (start < s.size() && (s[start] == '\n' || s[start] == '\r' ||
                              s[start] == ' ' || s[start] == '\t'))
  {
    ++start;
  }

  return s.substr(start);
}

inline std::string Exec(std::string_view cmd)
{
  std::array<char, 512> buffer{};
  std::string output;

#if defined(_WIN32)
  FILE* pipe = _popen(std::string(cmd).c_str(), "r");
#else
  FILE* pipe = popen(std::string(cmd).c_str(), "r");
#endif

  if (!pipe)
  {
    throw DockerError(std::format("Failed to run command: {}", cmd));
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
  {
    output += buffer.data();
  }

#if defined(_WIN32)
  const int rc = _pclose(pipe);
#else
  const int rc = pclose(pipe);
#endif

  if (rc != 0)
  {
    throw DockerError(std::format("Command failed (rc={}): {}\nOutput:\n{}", rc,
                                  cmd, output));
  }

  return Trim(output);
}

inline int System(std::string_view cmd)
{
  return std::system(std::string(cmd).c_str());
}

inline bool TryExec(std::string_view cmd, std::string* out = nullptr)
{
  try
  {
    auto result = Exec(cmd);
    if (out)
      *out = result;
    return true;
  }
  catch (...)
  {
    return false;
  }
}

inline void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline std::string UniqueSuffix()
{
  using namespace std::chrono;
  const auto now =
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count();
  return std::format("{}-{}", static_cast<long long>(getpid()), now);
}

inline std::string ParsePublishedPort(const std::string& dockerPortOutput)
{
  // example:
  // 0.0.0.0:49153
  // :::49153
  const auto pos = dockerPortOutput.rfind(':');
  if (pos == std::string::npos || pos + 1 >= dockerPortOutput.size())
  {
    throw DockerError(std::format("Could not parse published port from: {}",
                                  dockerPortOutput));
  }
  return dockerPortOutput.substr(pos + 1);
}

class ValkeyContainerBase : public ::testing::Test
{
public:
  std::string Host = "127.0.0.1";
  int Port = 0;
  std::vector<int> Ports;

  virtual ~ValkeyContainerBase() override
  {
    try
    {
      StopAll();
    }
    catch (...)
    {
      // never throw from destructor
    }
  }

protected:
  std::string Image = "valkey/valkey:latest";

  std::vector<std::string> ContainerIds;
  std::vector<std::string> ContainerNames;
  std::string NetworkName;

  ValkeyContainerBase() = default;

  void EnsureDockerAvailable()
  {
    std::string out;
    if (!TryExec("docker version --format \"{{.Server.Version}}\"", &out))
    {
      throw DockerError("Docker does not appear to be available.");
    }
  }

  void StopAll()
  {
    for (const auto& id : ContainerIds)
    {
      System(std::format("docker rm -f {} > /dev/null 2>&1", id));
    }
    ContainerIds.clear();

    if (!NetworkName.empty())
    {
      System(std::format("docker network rm {} > /dev/null 2>&1", NetworkName));
      NetworkName.clear();
    }
  }

  std::string Logs(const std::string& id) const
  {
    std::string out;
    TryExec(std::format("docker logs {}", id), &out);
    return out;
  }

  void WaitUntil(std::function<bool()> pred, std::chrono::milliseconds timeout,
                 std::string_view what)
  {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
      if (pred())
        return;
      SleepMs(200);
    }
    throw DockerError(std::format("Timed out waiting for {}", what));
  }

  int GetMappedPort(const std::string& id, int containerPort)
  {
    const auto out =
        Exec(std::format("docker port {} {}/tcp", id, containerPort));
    return std::stoi(ParsePublishedPort(out));
  }

bool PingContainerPort(const std::string& id, int containerPort)
{
  std::string out;
  return TryExec(
             std::format("docker exec {} valkey-cli -p {} ping", id, containerPort),
             &out) &&
         out == "PONG";
}

  bool PingHostPort(int hostPort)
  {
    std::string out;
    return TryExec(std::format("docker run --rm --network host {} valkey-cli "
                               "-h 127.0.0.1 -p {} ping",
                               Image, hostPort),
                   &out) &&
           out == "PONG";
  }

  bool ClusterInfoOk(const std::string& id, int nodePort)
  {
    std::string out;
    return TryExec(std::format("docker exec {} valkey-cli -p {} cluster info",
                               id, nodePort),
                   &out) &&
           out.find("cluster_state:ok") != std::string::npos;
  }

  void StartStandaloneContainer()
  {
    EnsureDockerAvailable();

    const auto suffix = UniqueSuffix();
    const std::string name = std::format("valkey-standalone-{}", suffix);

    const auto id =
        Exec(std::format("docker run -d --rm "
                         "--name {} "
                         "-p 0:6379 "
                         "{} "
                         "valkey-server --save \"\" --appendonly no",
                         name, Image));

    ContainerIds.push_back(id);
    ContainerNames.push_back(name);

    Port = GetMappedPort(id, 6379);
    Ports = {Port};

    WaitUntil(
    [&]() { return PingContainerPort(id, 6379); },
    std::chrono::seconds(15),
    "standalone valkey readiness");
  }

  template <int N> void StartClusterContainers()
  {
    static_assert(N >= 3, "ValkeyCluster requires at least 3 nodes.");
    EnsureDockerAvailable();

    const auto suffix = UniqueSuffix();
    NetworkName = std::format("valkey-cluster-net-{}", suffix);

    Exec(std::format("docker network create {}", NetworkName));

    ContainerIds.reserve(N);
    ContainerNames.reserve(N);
    Ports.reserve(N);

    constexpr int BasePort = 7000;
    constexpr int BaseBusPort = 17000;

    for (int i = 0; i < N; ++i)
    {
      const int port = BasePort + i;
      const int busPort = BaseBusPort + i;

      const std::string name = std::format("valkey-cluster-{}-{}", i, suffix);

      // Fixed host ports make cluster advertisement simple.
      // If you run tests in parallel, change these.
      /* const std::string cmd = std::format(
          "docker run -d --rm "
          "--name {} "
          "--network {} "
          "--network-alias node{} "
          "-p {0_port}:{0_port} "
          "-p {0_bus}:{0_bus} "
          "{} "
          "valkey-server "
          "--port {} "
          "--cluster-enabled yes "
          "--cluster-config-file nodes.conf "
          "--cluster-node-timeout 5000 "
          "--appendonly no "
          "--save \"\" "
          "--protected-mode no "
          "--bind 0.0.0.0 "
          "--cluster-announce-ip 127.0.0.1 "
          "--cluster-announce-port {} "
          "--cluster-announce-bus-port {}",
          name,
          NetworkName,
          i,
          port,
          port,
          busPort,
          Image); */

      // std::format does not support named placeholders, so build it plainly:
      const auto realCmd =
          std::format("docker run -d --rm "
                      "--name {} "
                      "--hostname node{} "
                      "--network {} "
                      "--network-alias node{} "
                      "-p {}:{} "
                      "-p {}:{} "
                      "{} "
                      "valkey-server "
                      "--port {} "
                      "--cluster-enabled yes "
                      "--cluster-config-file nodes.conf "
                      "--cluster-node-timeout 5000 "
                      "--appendonly no "
                      "--save \"\" "
                      "--protected-mode no "
                      "--bind 0.0.0.0 "
                      "--cluster-announce-hostname node{} "
                      "--cluster-announce-port {} "
                      "--cluster-announce-bus-port {}",
                      name, i, NetworkName, i, port, port, busPort, busPort,
                      Image, port, i, port, busPort);

      const auto id = Exec(realCmd);
      ContainerIds.push_back(id);
      ContainerNames.push_back(name);
      Ports.push_back(port);
    }

    for (int i = 0; i < N; ++i)
    {
      const auto& id = ContainerIds[i];
      const int port = BasePort + i;

      WaitUntil(
    [&]() { return PingContainerPort(id, port); },
    std::chrono::seconds(20),
    std::format("cluster node {} readiness", i));
    }

    // Build node list for cluster create.
    std::ostringstream nodes;
    for (int i = 0; i < N; ++i)
    {
      if (i)
        nodes << ' ';
      nodes << "node" << i << ':' << (BasePort + i);
    }

    // replicas = 0 for dead-simple cluster bringup.
    // If you want replicas later, make that configurable.
    const auto createCmd =
        std::format("docker exec {} sh -lc \"yes yes | valkey-cli --cluster "
                    "create {} --cluster-replicas 0\"",
                    ContainerIds[0], nodes.str());

    Exec(createCmd);

    WaitUntil([&]() { return ClusterInfoOk(ContainerIds[0], BasePort); },
              std::chrono::seconds(20), "cluster formation");

    Host = "127.0.0.1";
    Port = Ports.front();
  }
};

class ValkeyStandalone : public ValkeyContainerBase
{
public:
  ValkeyStandalone()
  {
    StartStandaloneContainer();
  }

  ~ValkeyStandalone() override = default;
};

template <int N> class ValkeyCluster : public ValkeyContainerBase
{
public:
  ValkeyCluster()
  {
    StartClusterContainers<N>();
  }

  ~ValkeyCluster() override = default;
};
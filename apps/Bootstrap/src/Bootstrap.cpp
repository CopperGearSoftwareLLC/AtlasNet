#include "Bootstrap.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "AtlasNet/AtlasNet.hpp"
#include "CommandTools.hpp"
#include "Docker/DockerIO.hpp"
#include "ImageDockerFiles.hpp"
#include "misc/String_utils.hpp"
#pragma once

void Bootstrap::Run()
{
	MultiNodeMode = !AtlasNet::Get().GetSettings().workers.empty();
	InitSwarm();
	if (MultiNodeMode)
	{
		CreateTLS();
		SendTLSToWorkers();
	}
	SetupNetwork();
	if (MultiNodeMode)
	{
		SetupRegistry();
	}
	BuildImages();
	DeployStack();
}

void Bootstrap::InitSwarm()
{
	auto isPortInUse = [](int port) -> bool
	{
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return true;
		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr("0.0.0.0");
		addr.sin_port = htons(port);
		bool inUse = bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0;
		close(sock);
		return inUse;
	};
	int swarmPort = _SWARM_DEFAULT_PORT;
	while (isPortInUse(swarmPort))
	{
		logger.WarningFormatted("⚠️ Port {} already in use, trying {}", swarmPort, swarmPort + 1);
		swarmPort++;
		if (swarmPort > 2400)
			throw std::runtime_error("No available ports found for swarm init (2377–2400)");
	}
	run_bash_with_sudo_fallback("docker swarm leave --force >/dev/null 2>&1", false);
	std::string infoResp = DockerIO::Get().request("GET", "/info");
	auto infoJson = Json::parse(infoResp);
	if (infoJson["Swarm"]["LocalNodeState"] == "active" &&
		infoJson["Swarm"]["ControlAvailable"] == true)
	{
		logger.Debug("Already a swarm manager — skipping init.");
		return;
	}

	logger.DebugFormatted("🧩 Initializing Docker Swarm on port {}", swarmPort);

	// Resolve configured network interface
	std::string preferredInterface = AtlasNet::Get().GetSettings().NetworkInterface;
	if (!MultiNodeMode)
		preferredInterface = "lo";	// if running locally then use the loop back interface 127.0.0.1
	std::string interfaceIP;

	// Collect available interfaces
	struct ifaddrs *ifaddr;
	if (getifaddrs(&ifaddr) == -1)
	{
		perror("getifaddrs");
		return;
	}

	struct InterfaceInfo
	{
		std::string name;
		std::string ip;
	};
	std::vector<InterfaceInfo> available;

	for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;

		char addr[INET_ADDRSTRLEN];
		void *in_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
		inet_ntop(AF_INET, in_addr, addr, sizeof(addr));

		available.push_back({ifa->ifa_name, addr});
	}
	freeifaddrs(ifaddr);

	// Try preferred interface if declared
	if (!preferredInterface.empty())
	{
		auto it = std::find_if(available.begin(), available.end(), [&](const InterfaceInfo &inf)
							   { return inf.name == preferredInterface; });

		if (it != available.end())
		{
			interfaceIP = it->ip;
			logger.DebugFormatted("✅ Using configured interface '{}' with IP '{}'",
								  preferredInterface, interfaceIP);
		}
		else
		{
			logger.WarningFormatted("⚠️ Configured interface '{}' not found or has no IPv4 address.",
									preferredInterface);
		}
	}

	// Ask user if needed
	if (interfaceIP.empty())
	{
		if (available.empty())
		{
			logger.Error("❌ No valid network interfaces with IPv4 addresses found.");
			return;
		}

		std::cout << "\nAvailable network interfaces:\n";
		for (size_t i = 0; i < available.size(); ++i)
			std::cout << "[" << i << "] " << available[i].name << " — " << available[i].ip << "\n";

		std::cout << "\nSelect interface number: ";
		int choice = -1;
		std::cin >> choice;

		if (choice < 0 || choice >= static_cast<int>(available.size()))
		{
			logger.Error("❌ Invalid selection.");
			return;
		}

		interfaceIP = available[choice].ip;
		preferredInterface = available[choice].name;

		logger.DebugFormatted("✅ Selected interface '{}' with IP '{}'", preferredInterface,
							  interfaceIP);
	}

	// Construct swarm init payload
	Json swarmInit = Json{{"ListenAddr", std::format("0.0.0.0:{}", swarmPort)},
						  {"AdvertiseAddr", std::format("{}:{}", interfaceIP, swarmPort)}};

	// Attempt swarm init
	std::string initResult = DockerIO::Get().request("POST", "/swarm/init", &swarmInit);
	Json parsed;
	try
	{
		parsed = Json::parse(initResult);
	}
	catch (...)
	{
		logger.ErrorFormatted("Invalid swarm init response: {}", initResult);
		return;
	}

	if (parsed.contains("message"))
	{
		logger.ErrorFormatted("❌ Swarm init failed: {}", parsed["message"].get<std::string>());
		return;
	}

	logger.DebugFormatted("Swarm init successful:\n{}", parsed.dump(4));

	// Refresh info to capture advertise address
	infoResp = DockerIO::Get().request("GET", "/info");
	infoJson = Json::parse(infoResp);
	ManagerAdvertiseAddress = infoJson["Swarm"]["NodeAddr"];
	logger.DebugFormatted("📡 Manager address set to {}", ManagerAdvertiseAddress);
}

void Bootstrap::CreateTLS()
{
	const std::string TLS_Directory = AtlasNet::Get().GetSettings().TlsDir.value_or("keys");
	const std::string certPath = TLS_Directory + "/server.crt";
	const std::string keyPath = TLS_Directory + "/server.key";
	const std::string configPath = TLS_Directory + "/san.cnf";
	bool needsRegeneration = false;
	if (std::filesystem::exists(certPath) && std::filesystem::exists(keyPath))
	{
		// Extract the IPs from the current certificate’s SAN
		std::string cmd = std::format(
			"openssl x509 -in '{}' -noout -text | grep -A1 'Subject Alternative Name'", certPath);
		CmdResult res = run_bash_with_sudo_fallback(cmd, true);

		if (res.exit_code == 0)
		{
			if (res.stderr_text.find(ManagerAdvertiseAddress) == std::string::npos)
			{
				logger.WarningFormatted(
					"⚠️ TLS certificate IP '{}' does not match current ManagerPcAdvertiseAddr '{}'. "
					"Regenerating...",
					res.stdout_text, ManagerAdvertiseAddress);
				needsRegeneration = true;
			}
		}
		else
		{
			logger.Warning("⚠️ Failed to read existing certificate SAN. Regenerating...");
			needsRegeneration = true;
		}
	}
	if (needsRegeneration)
	{
		try
		{
			std::filesystem::remove(certPath);
			std::filesystem::remove(keyPath);
			logger.Debug("🗑 Old certificate and key deleted due to IP change");
		}
		catch (const std::exception &e)
		{
			logger.ErrorFormatted("❌ Failed to delete old certificate: {}", e.what());
		}
	}
	if (!needsRegeneration && std::filesystem::exists(certPath) && std::filesystem::exists(keyPath))
	{
		logger.DebugFormatted("✅ Existing TLS certificate is valid for '{}'",
							  ManagerAdvertiseAddress);
		return;
	}
	logger.DebugFormatted("🔒 Generating new self-signed TLS certificate with SANs for IP '{}'",
						  ManagerAdvertiseAddress);
	std::filesystem::create_directories(TLS_Directory);

	std::string hostname = run_bash_with_sudo_fallback("hostname", false).stdout_text;
	hostname.erase(std::remove(hostname.begin(), hostname.end(), '\n'), hostname.end());
	std::ofstream config(configPath);
	config << "[ req ]\n";
	config << "default_bits       = 4096\n";
	config << "distinguished_name = req_distinguished_name\n";
	config << "req_extensions     = v3_req\n";
	config << "prompt             = no\n\n";
	config << "[ req_distinguished_name ]\n";
	config << "CN = " << hostname << "\n\n";
	config << "[ v3_req ]\n";
	config << "subjectAltName = @alt_names\n\n";
	config << "[ alt_names ]\n";
	config << "DNS.1 = " << hostname << "\n";
	config << "DNS.2 = localhost\n";
	config << "IP.1 = " << ManagerAdvertiseAddress << "\n";
	config.close();

	// --- Run OpenSSL ---
	std::string command = std::format(
		"openssl req -x509 -nodes -newkey rsa:4096 "
		"-keyout '{}' "
		"-out '{}' "
		"-days 3650 "
		"-config '{}' "
		"-extensions v3_req 2>&1",
		keyPath, certPath, configPath);
	int result = run_bash_with_sudo_fallback(command.c_str(), true).exit_code;
	if (result != 0)
	{
		logger.Error("❌ Failed to generate TLS certificate with SANs");
		throw std::runtime_error("TLS certificate generation failed");
	}

	logger.DebugFormatted(
		"✅ TLS certificate and key generated successfully with SANs (CN='{}', IP='{}')", hostname,
		ManagerAdvertiseAddress);
}
void Bootstrap::SendTLSToWorkers() {}

void Bootstrap::DeployStack()
{
	// e.g. "/tmp/atlasnet/"
	std::filesystem::create_directories(_DOCKER_TEMP_FILES_DIR);
	const auto stackymlFilePath = _DOCKER_TEMP_FILES_DIR + std::string("stack.yml");
	std::ofstream file(stackymlFilePath);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to save stack yml file");
	}
	file << ATLASNET_STACK;
	file.close();

	std::system(std::format("docker stack deploy --detach=true -c {} {}", stackymlFilePath,
							_ATLASNET_STACK_NAME)
					.c_str());
}
void Bootstrap::SetupNetwork()
{
	run_bash_with_sudo_fallback(
		std::format("docker network create -d overlay --attachable {}", _ATLASNET_NETWORK_NAME),
		false);
}
void Bootstrap::SetupRegistry()
{
	logger.Debug("🧱 Setting up Docker Registry service...");

	// Remove any existing registry service
	std::string cleanupCmd =
		std::format("docker service rm {} > /dev/null 2>&1", _REGISTRY_SERVICE_NAME);
	system(cleanupCmd.c_str());

	// Create a volume for registry data if it doesn't exist
	std::string createVolumeCmd = "docker volume create registry_data > /dev/null 2>&1";
	int volumeResult = system(createVolumeCmd.c_str());
	if (volumeResult != 0)
	{
		logger.Error("❌ Failed to create registry volume.");
		return;
	}
	const std::string TLS_Directory = AtlasNet::Get().GetSettings().TlsDir.value_or("keys");
	const std::string certPath = TLS_Directory + "/server.crt";
	const std::string keyPath = TLS_Directory + "/server.key";
	const std::string configPath = TLS_Directory + "/san.cnf";
	// Create the registry service connected to AtlasNet overlay
	std::string createServiceCmd = std::format(
		"docker service create "
		"--name {} "
		"--network {} "
		"-p {}:{} "
		"--mount type=volume,source=registry_data,target=/var/lib/registry "
		"--mount type=bind,source={},target=/certs/server.crt,readonly "
		"--mount type=bind,source={},target=/certs/server.key,readonly "
		"--env REGISTRY_HTTP_ADDR=0.0.0.0:5000 "
		"--env REGISTRY_HTTP_TLS_CERTIFICATE=/certs/server.crt "
		"--env REGISTRY_HTTP_TLS_KEY=/certs/server.key "
		"--constraint node.role==manager "
		"registry:2",
		_REGISTRY_SERVICE_NAME, _ATLASNET_NETWORK_NAME, _REGISTRY_PORT, _REGISTRY_PORT,
		std::filesystem::current_path().string() + "/" +
			certPath,  // or replace with your project root if needed
		std::filesystem::current_path().string() + "/" + keyPath);

	int createResult = system(createServiceCmd.c_str());
	if (createResult != 0)
	{
		logger.ErrorFormatted("❌ Failed to create registry service (error code {}).",
							  createResult);
		return;
	}

	// Verify registry service is running
	std::string checkCmd = std::format(
		"docker service ls --filter name={} --format '{{{{.Name}}}}: {{{{.Replicas}}}}'",
		_REGISTRY_SERVICE_NAME);
	CmdResult result = run_bash_with_sudo_fallback(checkCmd, true);
	if (result.exit_code == 0)
	{
		logger.DebugFormatted(
			"✅ Docker registry '{}' is running on port {} and connected to '{}'.",
			_REGISTRY_SERVICE_NAME, _REGISTRY_PORT, _ATLASNET_NETWORK_NAME);
	}
	else
	{
		logger.WarningFormatted("⚠️ Registry service '{}' may not be running yet:\n{}",
								_REGISTRY_SERVICE_NAME, result.stdout_text);
	}
}
void Bootstrap::BuildImages()
{
	BuildDockerImageLocally(GodDockerFile, _GOD_IMAGE_NAME);
	BuildDockerImageLocally(PartitionDockerFile, _PARTITION_IMAGE_NAME);
	BuildGameServer();	// must happen after partition
	BuildDockerImageLocally(DemiGodDockerFile, _DEMIGOD_IMAGE_NAME);
	BuildDockerImageLocally(ClusterDBDockerFile, _CLUSTERDB_IMAGE_NAME);
	BuildDockerImageLocally(GameCoordinatorDockerFile, _GAME_COORDINATOR_IMAGE_NAME);
}
void Bootstrap::BuildGameServer()
{
	const std::string SuperVisordConf = MacroParse(
		PartitionSuperVisordConf,
		{{"GAMESERVER_RUN_COMMAND", AtlasNet::Get().GetSettings().GameServerRunCommand}});

	const auto SuperVisorConfFilePath = _DOCKER_TEMP_FILES_DIR + std::string("supervisord.conf");
	std::ofstream SuperVisordConfFile(SuperVisorConfFilePath);
	SuperVisordConfFile << SuperVisordConf;
	SuperVisordConfFile.close();
	const std::string CopySupervisordConf = std::format("COPY {} {}\n",SuperVisorConfFilePath,_DOCKER_WORKDIR_);

	const auto GameServerDockerFile = AtlasNet::Get().GetSettings().GameServerDockerFile;
	std::ifstream input(GameServerDockerFile);
	if (!input.is_open())
	{
		throw std::runtime_error(
			std::format("Could not open Gameserver Dockerfile ({}) ", GameServerDockerFile));
	}
	std::string GSDockerContent;
	std::stringstream buffer;  // Create a stringstream object
	buffer << input.rdbuf();   // Read the entire file buffer into the stringstream
	GSDockerContent = buffer.str();
	GSDockerContent = MacroParse(
		GSDockerContent,
		{{"ATLASNET_PARTITION_INHERIT", std::format("FROM {} AS compose", _PARTITION_IMAGE_NAME)},
		 {"ATLASNET_PARTITION_ENTRYPOINT", CopySupervisordConf+PartitionEntryPoint},
		{"WORKDIR",_DOCKER_WORKDIR_}});

	BuildDockerImageLocally(GSDockerContent, _GAME_SERVER_IMAGE_NAME);
}
void Bootstrap::BuildDockerImageLocally(const std::string &DockerFileContent,
										const std::string &ImageName)
{
	// Build the image
	const auto dockerFile = _DOCKER_TEMP_FILES_DIR + ImageName + ".dockerfile";
	std::ofstream file(dockerFile);
	file << DockerFileContent;
	file.close();
	std::string buildCmd = std::format(
		"docker buildx build --progress=tty "
		"-f {} "
		"-t {} "
		" .",
		dockerFile, ImageName);

	logger.DebugFormatted("🏗️ Building image '{}' for arch '{}' ", ImageName, dockerFile);
	if (system(buildCmd.c_str()) != 0)
	{
		logger.ErrorFormatted("❌ Failed to build image '{}'", ImageName);
		throw std::runtime_error("Docker build failed");
	}

	logger.DebugFormatted("✅ Successfully built & pushed '{}'", ImageName);
}
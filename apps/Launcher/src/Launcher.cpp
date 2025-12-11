#include "Launcher.hpp"

#include <imgui.h>
#include <steam/steamtypes.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "AtlasNet/AtlasNet.hpp"
#include "CommandTools.hpp"
#include "Docker/DockerIO.hpp"
#include "ImageDockerFiles.hpp"
#include "MiscDockerFiles.hpp"
#include "misc/String_utils.hpp"
static std::optional<std::string> exec_capture(const std::string &cmd)
{
	std::array<char, 4096> buf{};
	std::string out;

	FILE *pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return std::nullopt;

	while (fgets(buf.data(), (int)buf.size(), pipe)) out += buf.data();

	const int rc = pclose(pipe);
	if (rc != 0)
		return std::nullopt;

	// trim trailing whitespace/newlines
	while (!out.empty() &&
		   (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '\t'))
		out.pop_back();

	return out;
}
void Launcher::OnStart() {
	// MultiNodeMode = !AtlasNet::Get().GetSettings().workers.empty();
	// InitSwarm();
	// SetupNetwork();
	// if (MultiNodeMode)
	//{
	//	CreateRegistryTLS();
	//	SendTLSToWorkers();
	//	SetupRegistry();
	//	SetupBuilder();
	// }
	// BuildImages();
	// DeployStack();
};
void Launcher::OnUpdate() {

};
void Launcher::OnRender()
{
	ImGui::ShowMetricsWindow();
	ImguiDockspace();
	ImguiBootstrap();
};
void Launcher::OnShutdown() {}
void Launcher::ImguiDockspace()
{
	// Remove window padding and borders
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove |
									ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("MainDockspace", nullptr, window_flags);
	ImGui::PopStyleVar(2);

	// Create the dockspace
	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// Draw background image behind everything in this window
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 win_pos = ImGui::GetWindowPos();
	ImVec2 win_size = ImGui::GetWindowSize();

	// Get center of the window
	ImVec2 center = ImVec2(win_pos.x + win_size.x * 0.5f, win_pos.y + win_size.y * 0.5f);

	// Assuming the image has known size
	const auto avail_space = win_size;
	const auto min_size = glm::min(avail_space.x, avail_space.y) * 0.33f;
	ImVec2 image_size(min_size, min_size);
	float BorderOffset = 10;
	float alpha = 80;
	ImVec2 image_min(center.x - (image_size.x * 0.5f), center.y - (image_size.y * 0.5f));
	ImVec2 image_max(center.x + (image_size.x * 0.5f), center.y + (image_size.y * 0.5f));
	ImVec2 border_min(center.x - (image_size.x * 0.5f + BorderOffset),
					  center.y - (image_size.y * 0.5f + BorderOffset));
	ImVec2 border_max(center.x + (image_size.x * 0.5f + BorderOffset),
					  center.y + (image_size.y * 0.5f + BorderOffset));

	// Draw it slightly transparent
	// draw_list->AddImage((ImTextureID)backgroundImage->GetHandle(), image_min, image_max,
	// ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
	draw_list->AddRect(border_min, border_max, IM_COL32(255, 255, 255, alpha), BorderOffset, 0,
					   2.0f);
	ImGui::End();
}
bool Launcher::ValidTLSCertificate()
{
	return true;
}

void Launcher::ImguiSwarmSetup()
{
	{
		struct ifaddrs *ifaddr;
		if (getifaddrs(&ifaddr) == -1)
		{
			perror("getifaddrs");
			return;
		}
		std::vector<const char *> labels;
		available_interfaces.clear();
		for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
				continue;

			char addr[INET_ADDRSTRLEN];
			void *in_addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, in_addr, addr, sizeof(addr));

			available_interfaces.push_back(
				{ifa->ifa_name, addr, std::format("{}-{}", ifa->ifa_name, addr)});
			labels.push_back(available_interfaces.back().displayLabel.c_str());
		}
		freeifaddrs(ifaddr);
		if (ImGui::Combo("##Network Interface", &interface_selected, labels.data(), labels.size()))
		{
		}
		ImGui::SameLine();
		if (ImGui::Button("Init"))
		{
			nlohmann::json body;
			body["ListenAddr"] = "0.0.0.0:2377";
			body["AdvertiseAddr"] =
				available_interfaces[interface_selected].ip + ":" + std::to_string(2377);

			const std::vector<std::string> headers = {"Content-Type: application/json"};

			std::string resp = DockerIO::Get().request("POST", "/swarm/init", &body, headers);
		}
		ImGui::SameLine();
		if (ImGui::Button("Shutdown"))
		{
			bool force = true;
			const std::string ep = force ? "/swarm/leave?force=1" : "/swarm/leave";
			DockerIO::Get().request("POST", ep, /*body*/ nullptr);
		}
	}

	auto GetSwarmStatus = [&swarm_status = swarm_status, &is_swarm_setup = is_swarm_setup,
						   &docker = DockerIO::Get()]()	 // capture your DockerIO instance
	{
		swarm_status = SwarmStatus();
		is_swarm_setup = false;

		// 1) Fast “is Docker up?” check: GET /_ping -> "OK"
		try
		{
			const std::string ping = docker.request("GET", "/_ping", /*body*/ nullptr);
			if (ping.find("OK") == std::string::npos)
				return;

			swarm_status.dockerReachable = true;
		}
		catch (...)
		{
			// socket not reachable / permission denied / daemon down
			return;
		}

		// 2) Fetch daemon info: GET /info
		nlohmann::json info;
		try
		{
			const std::string infoStr = docker.request("GET", "/info", /*body*/ nullptr);
			info = nlohmann::json::parse(infoStr);
		}
		catch (...)
		{
			return;
		}

		if (!info.is_object() || !info.contains("Swarm") || !info["Swarm"].is_object())
			return;

		const nlohmann::json &swarm = info["Swarm"];

		// Swarm active?
		const std::string localState = swarm.value("LocalNodeState", std::string{});
		swarm_status.swarmActive = (localState == "active");
		if (!swarm_status.swarmActive)
			return;

		// Manager capability?
		swarm_status.isManager = swarm.value("ControlAvailable", false);

		// Optional extra info
		if (swarm.contains("NodeID") && swarm["NodeID"].is_string())
			swarm_status.nodeID = swarm["NodeID"].get<std::string>();

		if (swarm.contains("RemoteManagers"))
		{
			const auto &rms = swarm["RemoteManagers"];
			if (rms.is_array())
			{
				// Prefer a clean "ip:port, ip:port" string when possible
				std::string joined;
				bool first = true;

				for (const auto &rm : rms)
				{
					std::string addr;
					if (rm.is_object() && rm.contains("Addr") && rm["Addr"].is_string())
						addr = rm["Addr"].get<std::string>();
					else if (rm.is_string())
						addr = rm.get<std::string>();

					if (!addr.empty())
					{
						if (!first)
							joined += ", ";
						joined += addr;
						first = false;
					}
				}

				swarm_status.managerAddr = !joined.empty() ? joined : rms.dump();
			}
			else
			{
				swarm_status.managerAddr = rms.dump();	// e.g. null/object/etc.
			}
		}

		is_swarm_setup =
			swarm_status.isManager && swarm_status.dockerReachable && swarm_status.swarmActive;
	};
	GetSwarmStatus();
	ImGui::BeginDisabled();
	ImGui::Checkbox("Docker reachable", &swarm_status.dockerReachable);
	ImGui::Checkbox("Swarm Active", &swarm_status.swarmActive);
	ImGui::Checkbox("Is Manager", &swarm_status.isManager);
	ImGui::EndDisabled();
	ImGui::Text("Node ID: %s", swarm_status.nodeID.c_str());
	ImGui::Text("Manager Address: %s", swarm_status.managerAddr.c_str());
}
void Launcher::ImguiWorkerSetup() 
{
	ImGui::BeginChild("worker list");
	ImGui::EndChild();
}
void Launcher::ImguiTLSGen()
{
	ImGui::InputText("Certificate Path", &TLSPath);
	ImGui::InputInt("TLS day validity", &TLSValidForNDays);
	if (ImGui::Button("Generate TLS Certificate"))
	{
		CreateTLS();
	}
	ImGui::SameLine();
	if (ValidTLSCertificate())
	{
		ImGui::TextColored(Yes_Color, "%s", "Valid TLS certificate");
	}
	else
	{
		ImGui::TextColored(No_Color, "%s", "No Certificate");
	}
}

void Launcher::ImguiBootstrap()
{
	ImGui::Begin("Bootstrap");

	ImGui::BeginChild("Global Options", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
	ImGui::Checkbox("Multi Node Setup", &Setup_For_MultiNode);
	ImGui::EndChild();
	ImGui::BeginChild("stack", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
	auto MakeChildWindow = [](const std::string &Name, bool enabled, auto &&func)
	{
		const auto ChildWindowFlagsDefault = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY;
		ImGui::BeginDisabled(!enabled);
		ImGui::BeginChild(Name.data(), ImVec2(0, 0), ChildWindowFlagsDefault);
		{
			std::forward<decltype(func)>(func)();
		}
		ImGui::EndChild();
		ImGui::EndDisabled();
	};
	bool enabled = true;
	{
		MakeChildWindow("Swarm Setup", enabled, [this]() { ImguiSwarmSetup(); });
		enabled &= is_swarm_setup;
		if (Setup_For_MultiNode)
		{
			const bool IsValidCertificate = ValidTLSCertificate();
			MakeChildWindow("TLS certificate", enabled, [this]() { ImguiTLSGen(); });
			MakeChildWindow("Workers Setup", enabled, [this]() { ImguiWorkerSetup(); });
		}
	}
	ImGui::EndChild();
	ImGui::End();
}
void Launcher::InitSwarm()
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
	if (!Setup_For_MultiNode)
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

void Launcher::CreateTLS()
{
	const std::string certPath = TLSPath + "/registry.crt";
	const std::string keyPath = TLSPath + "/registry.key";
	const std::string configPath = TLSPath + "/san.cnf";

	std::filesystem::create_directories(TLSPath);

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
	int result = system(command.c_str());
	if (result != 0)
	{
		logger.Error("❌ Failed to generate TLS certificate with SANs");
		throw std::runtime_error("TLS certificate generation failed");
	}
}
void Launcher::SendTLSToWorkers() {}
void Launcher::SetupBuilder()
{
	const std::string BuildKitConfig = std::format(R"(
	[registry."{}:{}"]
  		http = true
  		insecure = true
	)",
												   "registry", _REGISTRY_PORT);
	const std::string BuildKitConfigPath = _DOCKER_TEMP_FILES_DIR + std::string("buildkit.toml");
	WriteTextFile(BuildKitConfigPath, BuildKitConfig);
	std::system(std::format("docker buildx rm -f {}", _BUILDER_CONTAINER_NAME).c_str());

	std::string createCmd = std::format(
		"docker buildx create "
		"--name {} "
		"--driver docker-container "
		"--use "
		"--config {} "
		"--driver-opt network={} "
		"--driver-opt memory={}g "
		"--buildkitd-flags '--allow-insecure-entitlement network.host'",
		_BUILDER_CONTAINER_NAME, std::filesystem::absolute(BuildKitConfigPath).string(),
		_ATLASNET_NETWORK_NAME, AtlasNet::Get().GetSettings().BuilderMemoryGb.value_or(UINT32_MAX));
	std::system(createCmd.c_str());
	std::system(
		std::format("docker buildx inspect {} --bootstrap >/dev/null 2>&1", _BUILDER_CONTAINER_NAME)
			.c_str());
	std::system(std::format("docker network connect {} buildx_buildkit_{}0", _ATLASNET_NETWORK_NAME,
							_BUILDER_CONTAINER_NAME)
					.c_str());
}
void Launcher::DeployStack()
{
	// e.g. "/tmp/atlasnet/"
	std::filesystem::create_directories(_DOCKER_TEMP_FILES_DIR);
	const auto stackymlFilePath = _DOCKER_TEMP_FILES_DIR + std::string("stack.yml");
	std::ofstream file(stackymlFilePath);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to save stack yml file");
	}
	const std::string Registry_Addr_Opt =
		Setup_For_MultiNode ? std::format("{}:{}/", "registry", _REGISTRY_PORT) : "";
	file << MacroParse(ATLASNET_STACK, {{"REGISTRY_ADDR_OPT", Registry_Addr_Opt}});
	file.close();

	std::system(std::format("docker stack deploy --detach=true -c {} {}", stackymlFilePath,
							_ATLASNET_STACK_NAME)
					.c_str());
}
void Launcher::SetupNetwork()
{
	run_bash_with_sudo_fallback(
		std::format("docker network create -d overlay --attachable {}", _ATLASNET_NETWORK_NAME),
		false);
}
void Launcher::SetupRegistry()
{
	const auto stackymlFilePath = _DOCKER_TEMP_FILES_DIR + std::string("registry.yml");
	WriteTextFile(stackymlFilePath,
				  MacroParse(REGISTRY_STACK, {{"MANAGER_ADVERTISE_ADDR", ManagerAdvertiseAddress},
											  {"REGISTRY_PORT", std::to_string(_REGISTRY_PORT)}}));

	WriteTextFile(_DOCKER_TEMP_FILES_DIR + std::string("nginx.conf"), NGINX_CONFIG);
	std::system(
		std::format("docker stack deploy --detach=true -c {} {}", stackymlFilePath, "registry")
			.c_str());
}
void Launcher::BuildImages()
{
	if (Setup_For_MultiNode)
	{
		const auto arches = AtlasNet::Get().GetSettings().RuntimeArches;
		BuildDockerImageBuildX(GodDockerFile, _GOD_IMAGE_NAME, arches);
		BuildDockerImageBuildX(PartitionDockerFile, _PARTITION_IMAGE_NAME, arches);
		// BuildGameServer();	// must happen after partition
		BuildDockerImageBuildX(DemiGodDockerFile, _DEMIGOD_IMAGE_NAME, arches);
		BuildDockerImageBuildX(ClusterDBDockerFile, _CLUSTERDB_IMAGE_NAME, arches);
		BuildDockerImageBuildX(GameCoordinatorDockerFile, _GAME_COORDINATOR_IMAGE_NAME, arches);
	}
	else
	{
		BuildDockerImageLocally(GodDockerFile, _GOD_IMAGE_NAME);
		BuildDockerImageLocally(PartitionDockerFile, _PARTITION_IMAGE_NAME);
		BuildGameServer();	// must happen after partition
		BuildDockerImageLocally(DemiGodDockerFile, _DEMIGOD_IMAGE_NAME);
		BuildDockerImageLocally(ClusterDBDockerFile, _CLUSTERDB_IMAGE_NAME);
		BuildDockerImageLocally(GameCoordinatorDockerFile, _GAME_COORDINATOR_IMAGE_NAME);
	}
}
void Launcher::BuildGameServer()
{
	/*
	const std::string SuperVisordConf = MacroParse(
		PartitionSuperVisordConf,
		{{"GAMESERVER_RUN_COMMAND", AtlasNet::Get().GetSettings().GameServerRunCommand}});

	const auto SuperVisorConfFilePath = _DOCKER_TEMP_FILES_DIR + std::string("supervisord.conf");
	std::ofstream SuperVisordConfFile(SuperVisorConfFilePath);
	SuperVisordConfFile << SuperVisordConf;
	SuperVisordConfFile.close();
	const std::string CopySupervisordConf =
		std::format("COPY {} {}\n", SuperVisorConfFilePath, _DOCKER_WORKDIR_);

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
		 {"ATLASNET_PARTITION_ENTRYPOINT", CopySupervisordConf + PartitionEntryPoint},
		 {"WORKDIR", _DOCKER_WORKDIR_}});

	BuildDockerImageLocally(GSDockerContent, _GAME_SERVER_IMAGE_NAME);*/
}
void Launcher::BuildDockerImageBuildX(const std::string &DockerFileContent,
									  const std::string &ImageName,
									  const std::unordered_set<std::string> &arches)
{
	// Full registry image name: <ManagerIPAddr>/registry_registry/<ImageName>:latest
	const std::string registryImage =
		std::format("{}:{}/{}:{}", "registry", _REGISTRY_PORT, ImageName, "latest");

	// Write Dockerfile
	const auto dockerFile = _DOCKER_TEMP_FILES_DIR + ImageName + ".dockerfile";
	std::ofstream file(dockerFile);
	if (!file.is_open())
		throw std::runtime_error("Failed to write dockerfile");

	file << DockerFileContent;
	file.close();

	// Build platform string
	std::string platformList;
	for (auto &arch : arches)
	{
		if (!platformList.empty())
			platformList += ",";
		platformList += arch;  // example: linux/amd64,linux/arm64
	}

	// Buildx command
	const std::string CacheDIr = std::filesystem::absolute(
		std::format("{}{}/{}", _DOCKER_TEMP_FILES_DIR, "buildkit-cache", ImageName));
	std::string buildCmd = std::format(
		"docker buildx build "
		"--progress=tty "
		"--builder {} "
		"--push "
		"--cache-from type=local,src={} "
		"--cache-to   type=local,dest={},mode=max "
		"--platform={} "
		"-f {} "
		"-t {} "
		" .",
		_BUILDER_CONTAINER_NAME, CacheDIr, CacheDIr, platformList, dockerFile, registryImage);

	logger.DebugFormatted("🏗️ Building & pushing image '{}' → '{}'", ImageName, registryImage);

	if (system(buildCmd.c_str()) != 0)
	{
		logger.ErrorFormatted("❌ Failed to build/push image '{}'", ImageName);
		throw std::runtime_error("Docker buildx failed");
	}

	logger.DebugFormatted("✅ Successfully built & pushed '{}'", registryImage);
}

void Launcher::BuildDockerImageLocally(const std::string &DockerFileContent,
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
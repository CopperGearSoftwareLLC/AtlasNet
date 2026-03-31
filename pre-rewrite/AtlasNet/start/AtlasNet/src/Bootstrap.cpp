#include "Bootstrap.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "BuiltInDbDockerFiles.hpp"
#include "CartographDockerFiles.hpp"
#include "CommandTools.hpp"
#include "DockerIO.hpp"
#include "ImageDockerFiles.hpp"
#include "Misc/String_utils.hpp"
#include "Utils.hpp"
#include "nlohmann/json_fwd.hpp"
#include "pch.hpp"

static void WriteFile(std::filesystem::path file_path, const std::string_view str)
{
	std::filesystem::create_directories(file_path.parent_path());
	std::ofstream file(file_path);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to write to file");
	}
	file << str;
	file.close();
}
static std::string ReadFile(std::filesystem::path file_path)
{
	std::ifstream input(file_path);
	if (!input.is_open())
	{
		throw std::runtime_error(std::format("Could not read file ({}) ", file_path.string()));
	}

	std::stringstream buffer;  // Create a stringstream object
	buffer << input.rdbuf();   // Read the entire file buffer into the stringstream
	return buffer.str();
}
static Json ReadJsonFile(std::filesystem::path file_path)
{
	return Json::parse(ReadFile(file_path));
}
Bootstrap::Settings Bootstrap::ParseSettingsFile(const std::filesystem::path &file)
{
	Settings settings;
	std::ifstream settingsFile(file);
	std::cerr << "Reading settings file at " << file.string() << std::endl;
	if (!settingsFile.is_open())
	{
		throw std::runtime_error(
			std::format("Unable to find {}. set the path to your settings file with {} <path>",
						file.string(), BOOTSTRAP_SETTINGS_FILE_FLAG));
	}

	Ordered_Json parsedJson;
	try
	{
		std::string contents((std::istreambuf_iterator<char>(settingsFile)),
							 std::istreambuf_iterator<char>());
		parsedJson = Ordered_Json::parse(contents);
	}
	catch (...)
	{
		std::cerr << std::format("Failed to parse file {}\n", file.string());
		throw std::runtime_error("fuck");
	}
	settingsFile.close();
	auto IsEntryValid = [](const Json &json, const std::string &str)
	{ return json.contains(str) && !json[str].is_null(); };
	if (IsEntryValid(parsedJson, "Workers"))
	{
		for (const auto &workerJ : parsedJson["Workers"])
		{
			Settings::Worker worker;
			worker.MAC = workerJ["MACAddress"];
			worker.Name = workerJ["Name"];
			settings.workers.push_back(worker);
		}
	}
	if (IsEntryValid(parsedJson, "GameServerTaskFile"))
	{
		settings.GameServerTaskFile = parsedJson["GameServerTaskFile"].get<std::string>();
	}

	// --- GameServerRunCmd ---
	settings.RuntimeArches;
	for (const auto &arch : parsedJson["RuntimeArches"])
	{
		settings.RuntimeArches.insert(arch);
	}

	settings.NetworkInterface =
		IsEntryValid(parsedJson, "NetworkInterface") ? parsedJson["NetworkInterface"] : "";

	if (IsEntryValid(parsedJson, "TLSDirectory"))
	{
		settings.TlsDir = parsedJson["TLSDirectory"].get<std::string>();
	}
	if (IsEntryValid(parsedJson["BuiltInDB"], "Enable"))
	{
		settings.BuiltInDBopt.Enable = parsedJson["BuiltInDB"]["Enable"];
	}
	if (IsEntryValid(parsedJson["BuiltInDB"]["Transient"], "MasterReplicas"))
	{
		settings.BuiltInDBopt.transient.MasterReplicas =
			parsedJson["BuiltInDB"]["Transient"]["MasterReplicas"];
	}
	if (IsEntryValid(parsedJson["BuiltInDB"]["Transient"], "SlaveReplicas"))
	{
		settings.BuiltInDBopt.transient.SlaveReplicas =
			parsedJson["BuiltInDB"]["Transient"]["SlaveReplicas"];
	}
	return settings;
}
Bootstrap::Task Bootstrap::ParseTaskFile(std::filesystem::path file_path)
{
	const Json json = ReadJsonFile(file_path);

	Task task;
	task.task_path = file_path;

	// build.dockerfile
	if (json.contains("build") && json["build"].contains("dockerfile"))
		task.build.dockerfile = json["build"]["dockerfile"].get<std::string>();

	// run.*
	if (json.contains("run"))
	{
		const Json &run = json["run"];

		// workdir
		if (run.contains("workdir") && run["workdir"].is_string())
			task.run.workdir = run["workdir"].get<std::string>();

		// command: allow string OR array
		if (run.contains("command"))
		{
			const Json &cmd = run["command"];
			if (cmd.is_string())
			{
				task.run.command = cmd.get<std::string>();
			}
			else if (cmd.is_array() && !cmd.empty())
			{
				// first element is executable, remaining become leading args
				task.run.command = cmd[0].get<std::string>();
				for (std::size_t i = 1; i < cmd.size(); ++i)
					task.run.args.push_back(cmd[i].get<std::string>());
			}
		}

		// args
		if (run.contains("args") && run["args"].is_array())
		{
			for (const auto &a : run["args"]) task.run.args.push_back(a.get<std::string>());
		}

		// env: object -> vector<pair>
		if (run.contains("env") && run["env"].is_object())
		{
			for (auto it = run["env"].begin(); it != run["env"].end(); ++it)
			{
				// store everything as string (common for env vars)
				std::string value;
				if (it.value().is_string())
					value = it.value().get<std::string>();
				else
					value = it.value().dump();

				task.run.env.emplace_back(it.key(), std::move(value));
			}
		}

		// ports: array of { container, protocol }
		if (run.contains("ports") && run["ports"].is_array())
		{
			for (const auto &p : run["ports"])
			{
				Task::Run::Port port;
				if (p.contains("container"))
					port.container = static_cast<uint16_t>(p["container"].get<int>());
				if (p.contains("protocol"))
					port.protocol = p["protocol"].get<std::string>();
				task.run.ports.push_back(std::move(port));
			}
		}
	}

	return task;
}
void Bootstrap::Run(const RunArgs &args)
{
	settings = ParseSettingsFile(args.AtlasNetSettingsPath.value_or("./AtlasNetSettings.json"));
	std::filesystem::path game_server_task_file_path =
		args.AtlasNetSettingsPath.value_or("./AtlasNetSettings").parent_path().string() + "/" +
		settings.GameServerTaskFile;
	game_server_task = ParseTaskFile(game_server_task_file_path);
	MultiNodeMode = !settings.workers.empty();
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
	std::string preferredInterface = settings.NetworkInterface;
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
	const std::string TLS_Directory = settings.TlsDir.value_or("keys");
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
	const std::string ProcessedStackFile = MacroParse(
		ATLASNET_STACK, {{"BUILTIN_DB_STACK", settings.BuiltInDBopt.Enable ? BuiltInDbStack : ""}});
	// e.g. "/tmp/atlasnet/"
	std::filesystem::create_directories(_DOCKER_TEMP_FILES_DIR);
	const auto stackymlFilePath = _DOCKER_TEMP_FILES_DIR + std::string("stack.yml");
	std::ofstream file(stackymlFilePath);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to save stack yml file");
	}
	file << ProcessedStackFile;
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
	const std::string TLS_Directory = settings.TlsDir.value_or("keys");
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
	QueueDockerImageBuild(WatchDogDockerFile, _WATCHDOG_IMAGE_NAME);
	QueueDockerImageBuild(ShardDockerFile, _SHARD_IMAGE_NAME);

	QueueDockerImageBuild(ProxyDockerFile, _PROXY_IMAGE_NAME);
	// BuildDockerImageLocally(GameCoordinatorDockerFile, _GAME_COORDINATOR_IMAGE_NAME);
	QueueDockerImageBuild(CartographDockerFile, _CARTOGRAPH_IMAGE_NAME);

	BuildAllImages();

	//BuildDockerImageLocally(atlasDockerfileContents, "atlasnetsdk");
	//BuildDockerImageLocally(WatchDogDockerFile, _WATCHDOG_IMAGE_NAME);
	//BuildDockerImageLocally(ShardDockerFile, _SHARD_IMAGE_NAME);
	BuildGameServer();	// must happen after partition
	//					// BuildAllImages();
	//BuildDockerImageLocally(ProxyDockerFile, _PROXY_IMAGE_NAME);
	////// BuildDockerImageLocally(GameCoordinatorDockerFile, _GAME_COORDINATOR_IMAGE_NAME);
	//BuildDockerImageLocally(CartographDockerFile, _CARTOGRAPH_IMAGE_NAME);
}

void Bootstrap::BuildGameServer()
{
	const auto dockerfilepath =
		game_server_task.task_path.parent_path().string() + "/" + game_server_task.build.dockerfile;
	auto DockerFileContents = ReadFile(dockerfilepath);

	std::string runCommand = std::format("{} ", game_server_task.run.command);
	for (const auto arg : game_server_task.run.args)
	{
		runCommand += std::format("{} ", arg);
	}
	DockerFileContents =
		MacroParse(DockerFileContents, {{"GAME_SERVER_ENTRYPOINT", GameServerEntryPoint},
										{"GAME_SERVER_RUN_COMMAND", runCommand},
										{"GAME_SERVER_WORK_DIR", game_server_task.run.workdir}});

	std::cerr << DockerFileContents << " \nat : " << game_server_task.task_path.parent_path()
			  << std::endl;
	BuildDockerImageLocally(DockerFileContents, _GAME_SERVER_IMAGE_NAME,
							game_server_task.task_path.parent_path());
	// buildo(DockerFileContents, _GAME_SERVER_IMAGE_NAME,
	//					  game_server_task.task_path.parent_path(), {_SHARD_IMAGE_NAME});
}
void Bootstrap::BuildDockerImageLocally(const std::string &DockerFileContent,
										const std::string &ImageName,
										std::filesystem::path workingDir)
{
	// Build the image
	const auto dockerFile = _DOCKER_TEMP_FILES_DIR + ImageName + ".dockerfile";
	WriteFile(dockerFile, DockerFileContent);
	std::string buildCmd = std::format(
		"cd {} && docker buildx build "
		"-f {} "
		"-t {} "
		" .",
		std::filesystem::absolute(workingDir).string(),
		std::filesystem::absolute(dockerFile).string(), ImageName);

	logger.DebugFormatted("🏗️ Building image '{}' for arch '{}' ", ImageName, dockerFile);
	if (system(buildCmd.c_str()) != 0)
	{
		logger.ErrorFormatted("❌ Failed to build image '{}'", ImageName);
		throw std::runtime_error("Docker build failed");
	}

	logger.DebugFormatted("✅ Successfully built & pushed '{}'", ImageName);
}
void Bootstrap::QueueDockerImageBuild(const std::string &DockerFileContent,
									  const std::string &ImageName,
									  std::filesystem::path workingDir,
									  std::vector<std::string> DependsOn)
{
	const auto dockerFile = _DOCKER_TEMP_FILES_DIR + ImageName + ".dockerfile";
	WriteFile(dockerFile, DockerFileContent);
	ImageBuildOpt b;
	b.dockerfile = dockerFile;
	b.workingDir = std::move(workingDir);
	b.ImageName = ImageName;
	b.DependsOn = std::move(DependsOn);
	images_to_build.push_back(b);
	std::cout << "Queued " << ImageName << std::endl;
}
void Bootstrap::BuildAllImages()
{
	if (images_to_build.empty())
		return;

	const auto atlasDockerfilePath =
		std::filesystem::path(_DOCKER_TEMP_FILES_DIR) / "atlasnetsdk.dockerfile";
	std::string atlasDockerfileContents =
		R"(# Minimal atlasnetsdk stage
FROM scratch AS atlasnetsdk
COPY . /)";
	WriteFile(atlasDockerfilePath.string(), atlasDockerfileContents);

	// 1️⃣ First, build atlasnetsdk as a regular docker image
	std::string atlasBuildCmd =
		std::format("docker build --load -f {} -t atlasnetsdk {}", atlasDockerfilePath.string(),
					GetAtlasNetPath().string());
	logger.DebugFormatted("🏗️ Building atlasnetsdk first: {}", atlasBuildCmd);

	if (system(atlasBuildCmd.c_str()) != 0)
	{
		logger.Error("❌ atlasnetsdk build failed");
		throw std::runtime_error("atlasnetsdk build failed");
	}

	// 2️⃣ Now generate docker-bake.json for the rest of the images
	nlohmann::json bakeJson;
	nlohmann::json targetsArray = nlohmann::json::array();
	targetsArray.push_back("atlasnetsdk");
	std::unordered_set<std::string> ReadDirs;
	for (const auto &img : images_to_build)
	{
		nlohmann::json target;
		target["context"] = std::filesystem::absolute(img.workingDir).string();
		target["dockerfile"] = std::filesystem::absolute(img.dockerfile).string();
		target["tags"] = {img.ImageName};
		target["push"] = false;
		target["load"] = true;
		ReadDirs.insert(std::filesystem::absolute(img.workingDir).string());
		//target["memory"] = "2g";

		nlohmann::json depends = nlohmann::json::array();
		nlohmann::json buildContexts = nlohmann::json::object();

		// Always depend on atlasnetsdk
		depends.push_back("atlasnetsdk");
		buildContexts["atlasnetsdk"] = "image:atlasnetsdk";	 // reference the already built image

		// Add other dependencies from img.DependsOn
		for (const auto &dep : img.DependsOn)
		{
			depends.push_back(dep);
			buildContexts[dep] = "target:" + dep;
		}

		target["depends-on"] = depends;
		//target["shm-size"] = "2g";
		target["build-contexts"] = buildContexts;

		bakeJson["target"][img.ImageName] = target;
		targetsArray.push_back(img.ImageName);
	}

	bakeJson["group"]["default"]["targets"] = targetsArray;

	// 3️⃣ Write JSON to temp file
	const auto bakeFile = std::filesystem::path(_DOCKER_TEMP_FILES_DIR) / "docker-bake.json";
	WriteFile(bakeFile.string(), bakeJson.dump(4));
	logger.DebugFormatted("📦 Generated docker-bake.json at '{}'", bakeFile.string());

	// 4️⃣ Build remaining images via BuildKit bake
	std::string bakeCmd =
		std::format("docker buildx bake --file {} --allow=fs.read={} --allow=fs.read={} ",
					bakeFile.string(), GetAtlasNetPath().string(), _DOCKER_TEMP_FILES_DIR);
	for (const auto &readDir : ReadDirs)
	{
		bakeCmd += std::format("--allow=fs.read={} ", readDir);
	}

	for (const auto &image : images_to_build)
	{
		bakeCmd += " " + image.ImageName;
	}

	logger.DebugFormatted("🏗️ Running: {}", bakeCmd);

	if (system(bakeCmd.c_str()) != 0)
	{
		logger.Error("❌ One or more image builds failed");
		throw std::runtime_error("Buildx bake failed");
	}

	logger.Debug("✅ All images built successfully");
}

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include "Log.hpp"
class Bootstrap
{
	struct Settings
	{
		struct Worker
		{
			std::string Name;
			std::string IP;
		};
		std::vector<Worker> workers;
		std::unordered_set<std::string> RuntimeArches;
		std::string BuildCacheDir;
		std::string NetworkInterface;
		std::optional<uint32_t> BuilderMemoryGb;
		std::optional<std::string> TlsDir;

		std::string GameServerTaskFile;
		std::string GameServerRunCommand;
		std::string GameServerBuildDir;
	} settings;
	static Settings ParseSettingsFile(const std::filesystem::path& file);
	struct Task
	{
		using EnviromentVar = std::pair<std::string, std::string>;
		struct Build
		{
			std::string dockerfile;
		} build;
		struct Run
		{
			struct Port
			{
				uint16_t container = 0;
				std::string protocol;  // "tcp" / "udp"
			};

			std::filesystem::path workdir;
			std::string command;
			std::vector<std::string> args;
			std::vector<EnviromentVar> env;
			std::vector<Port> ports;
		} run;
		std::filesystem::path task_path;
	};
	Task game_server_task;

	std::string RegistryTagHeader = "";

	bool MultiNodeMode = false;
	std::string ManagerAdvertiseAddress;
	Log logger = Log("");
	void InitSwarm();
	void CreateTLS();
	void SendTLSToWorkers();
	void JoinWorkers();
	void SetupBuilder();
	void SetupNetwork();
	void SetupRegistry();
	void BuildImages();
	void BuildGameServer();
	void DeployStack();

	void BuildDockerImageLocally(
		const std::string& DockerFileContent, const std::string& ImageName,
		std::filesystem::path workingDir = std::filesystem::current_path());
	void BuildDockerImageBuildX(const std::string& DockerFileContent, const std::string& ImageName,
								const std::unordered_set<std::string>& arches);

	static Task ParseTaskFile(std::filesystem::path file_path);

   public:
	struct RunArgs
	{
		std::optional<std::filesystem::path> AtlasNetSettingsPath;
	} run_args;
	void Run(const RunArgs&);
};
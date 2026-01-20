#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include "Utils.hpp"
#include "Log.hpp"
class Bootstrap
{
	struct Settings
	{
		struct Worker
		{
			std::string Name;
			std::string MAC;
		};
		std::vector<Worker> workers;
		std::unordered_set<std::string> RuntimeArches;
		std::string NetworkInterface;
		std::optional<std::string> TlsDir;

		std::string GameServerTaskFile;

		struct BuiltInDBOpt
		{
			bool Enable;
			struct Transient
			{
				uint32_t MasterReplicas = 1;
				uint32_t SlaveReplicas = 1;
			} transient;
		} BuiltInDBopt;

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

	struct ImageBuildOpt
	{
		std::filesystem::path dockerfile;
		std::string ImageName;
		std::filesystem::path workingDir;
		std::vector<std::string> DependsOn;
	};
	std::vector<ImageBuildOpt> images_to_build;
	void QueueDockerImageBuild(const std::string& DockerFileContent, const std::string& ImageName,
							   std::filesystem::path workingDir = GetEXEPath().parent_path(),std::vector<std::string> DependsOn = {});
	void BuildAllImages();

	void BuildDockerImageLocally(
		const std::string& DockerFileContent, const std::string& ImageName,
		std::filesystem::path workingDir = GetEXEPath().parent_path());
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
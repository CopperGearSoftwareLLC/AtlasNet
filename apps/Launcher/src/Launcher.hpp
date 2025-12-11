#pragma once

#include <imgui.h>
#include <steam/steamtypes.h>

#include "App.h"
#include "Debug/Log.hpp"
class Launcher : public App
{
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

	void BuildDockerImageLocally(const std::string& DockerFileContent,
								 const std::string& ImageName);
	void BuildDockerImageBuildX(const std::string& DockerFileContent, const std::string& ImageName,
								const std::unordered_set<std::string>& arches);

	void OnStart() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnShutdown() override;
	ImVec4 Yes_Color = ImVec4(0.2, 1, 0.2, 1);
	ImVec4 No_Color = ImVec4(1, 0.2, 0.2, 1);

	std::string TLSPath = "temp/keys/";
	int32 TLSValidForNDays = 365;
	bool ValidTLSCertificate();

	void ImguiDockspace();
	void ImguiBootstrap();

	bool Setup_For_MultiNode = false;
	struct NetworkInterfaceInfo
	{
		std::string name;
		std::string ip;
		std::string displayLabel;
	};
	std::vector<NetworkInterfaceInfo> available_interfaces;
	int32 interface_selected = 0;
	bool is_swarm_setup = false;
	struct SwarmStatus
	{
		bool dockerReachable = false;  // docker daemon responds
		bool swarmActive = false;	   // Swarm: active
		bool isManager = false;		   // Swarm: active + ControlAvailable: true
		std::string nodeID;
		std::string managerAddr;
	} swarm_status;
	void ImguiSwarmSetup();
	void ImguiWorkerSetup();
	void ImguiTLSGen();

   public:
	Launcher() : App(AppProperties{.AppName = "AtlasNet Launcher", .imgui_docking_enable = true}) {}
	virtual ~Launcher() = default;
};
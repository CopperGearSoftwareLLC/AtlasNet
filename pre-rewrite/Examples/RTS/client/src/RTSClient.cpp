#include "RTSClient.hpp"

#include <algorithm>
#include <boost/geometry/algorithms/detail/expand/interface.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "AtlasNetClient.hpp"
#include "Debug/DebugDraw.hpp"
#include "Entities/CameraPivot.hpp"
#include "Entities/DummyEntity.hpp"
#include "Entities/FloorEntity.hpp"
#include "Entities/WorkerEntity.hpp"
#include "Entity.hpp"
#include "EntityID.hpp"
#include "GLFW/glfw3.h"
#include "Global/pch.hpp"
#include "PlayerColors.hpp"
#include "Window.hpp"
#include "World.hpp"
#include "commands/GameStateCommand.hpp"
#include "commands/PlayerAssignStateCommand.hpp"
#include "commands/PlayerCameraMoveCommand.hpp"
#include "commands/WorkerMoveCommand.hpp"
#include "imgui.h"
#include "imgui_internal.h"

int main(int argc, char** argv)
{
	IPAddress address;
	if (argc > 1)
	{
		address.Parse(std::string(argv[1]));
	}
	else
	{
		address = IPAddress::MakeLocalHost(_PORT_PROXY_PUBLISHED);
	}
	RTSClient::Get().Run(address);
	return 0;
}
void RTSClient::Run(const IPAddress& address)
{
	Window::Ensure();
	DebugDraw::Ensure();
	Window::Get().PreRender();

	camera = &World::Get().CreateEntity<Camera>();

	camera->SetPerspective(false);
	camera->transform.Pos = {0, 0, 200};
	RenderScreenText("Connecting to server...", vec4(1, 1, 0, 1));
	Window::Get().PostRender();
	glfwPollEvents();
	AtlasNetClient::Get().GetCommandBus().Subscribe<PlayerAssignStateCommand>(
		[this](const auto& recvHeader, const auto& command)
		{ OnPlayerAssignStateCommand(recvHeader, command); });
	AtlasNetClient::Get().GetCommandBus().Subscribe<GameStateCommand>(
		[this](const auto& recvHeader, const auto& command)
		{ onGameStateCommand(recvHeader, command); });
	AtlasNetClient::InitializeProperties properties;
	logger.DebugFormatted("Connecting To AtlasNet at {}", address.ToString());
	properties.AtlasNetProxyIP = address;
	AtlasNetClient::Get().Initialize(properties);

	auto& CameraDummy = World::Get().CreateEntity<CameraPivot>();
	CameraDummy.Own(camera->GetID());
	CameraDummy.transform.Rot = quat(radians(vec3(-35, -45, 0)));
	cameraPivot = &CameraDummy;
	// std::mt19937 rng(std::random_device{}());
	// std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
	//
	// for (int i = 0; i < 10; i++)
	//{
	//	auto& worker = World::Get().CreateEntity<Worker>();
	//	worker.transform.Pos = vec3(dist(rng), 0.0f, dist(rng));
	//}
	World::Get().CreateEntity<FloorEntity>().transform.Scale = vec3(100, 1, 100);
	using clock = std::chrono::high_resolution_clock;

	auto lastTime = clock::now();

	constexpr float targetFPS = 120.0f;
	constexpr float targetFrameTime = 1.0f / targetFPS;

	// --- fixed step (20 Hz) ---
	constexpr float fixedStep = 1.0f / 20.0f;  // 0.05s
	float accumulator = 0.0f;

	while (!ShouldShutdown)
	{
		auto currentTime = clock::now();
		std::chrono::duration<float> delta = currentTime - lastTime;
		float deltaTime = delta.count();
		lastTime = currentTime;

		// Clamp delta to avoid spiral of death (optional but recommended)
		if (deltaTime > 0.25f)
			deltaTime = 0.25f;

		accumulator += deltaTime;

		// --- RUNS EXACTLY 20 TIMES PER SECOND ---
		while (accumulator >= fixedStep)
		{
			FixedUpdate(fixedStep);	 // <<--- your 20 Hz logic here
			accumulator -= fixedStep;
		}

		// Optional: interpolation factor (if you need smooth rendering)
		float alpha = accumulator / fixedStep;

		// Frame cap (your existing logic)
		if (deltaTime < targetFrameTime)
		{
			std::this_thread::sleep_for(std::chrono::duration<float>(targetFrameTime - deltaTime));
		}

		Window::Get().PreRender();
		Update(deltaTime);	// variable rate (input, camera, etc.)
		Render(deltaTime);	// can use alpha if interpolating
		Window::Get().PostRender();

		for (const auto& worker : WorkersToParse)
		{
			vec3 worldPos = worker.Position;
			std::swap(worldPos.y, worldPos.z);
			if (!RemoteID2LocalID.contains(worker.ID))
			{
				auto& newWorker = World::Get().CreateEntity<Worker>();
				RemoteID2LocalID[worker.ID] = newWorker.GetID();
				LocalID2RemoteID[newWorker.GetID()] = worker.ID;
			}
			Worker& localWorker = World::Get().GetEntity<Worker>(RemoteID2LocalID[worker.ID]);
			// localWorker.SetColorOverride(PlayerTeamsToColor(worker.team));
			localWorker.transform.Pos = worldPos;
		}
		// for (const auto& [remoteID, localID] : RemoteID2LocalID)
		//{
		//	if (std::none_of(WorkersToParse.begin(), WorkersToParse.end(),
		//					 [&](const WorkerData& w) { return w.ID == remoteID; }))
		//	{
		//		World::Get().DeleteEntity(localID);
		//		RemoteID2LocalID.erase(remoteID);
		//	}
		// }
		WorkersToParse.clear();
	}
}

void RTSClient::Render(float deltaTime)
{
	World::Get().DebugMenu();
	World::Get().RenderAll();

	DebugDraw::Get().Flush(camera->GetViewProjMat());

	if (myAssignedTeam.has_value())
	{
		RenderScreenText(
			std::format("Assigned Team {}", PlayerTeamToString(myAssignedTeam.value())),
			vec4(PlayerTeamsToColor(*myAssignedTeam), 1));
	}
}
glm::vec2 RightClickStartNDC;
bool bRightClickDragging = false;
glm::vec3 NDCToWorldXZ(const glm::vec2& ndc, const glm::mat4& viewProj)
{
	// Invert the viewProj matrix
	glm::mat4 invVP = glm::inverse(viewProj);

	// Start with NDC, z = 0 (near plane)
	glm::vec4 nearPoint = invVP * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
	glm::vec4 farPoint = invVP * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);

	// Perspective divide
	nearPoint /= nearPoint.w;
	farPoint /= farPoint.w;

	// Ray from near → far
	glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));
	glm::vec3 rayOrigin = glm::vec3(nearPoint);

	// Intersect with XZ plane at y = 0
	if (rayDir.y == 0.0f)
		return rayOrigin;  // avoid division by zero

	float t = -rayOrigin.y / rayDir.y;
	return rayOrigin + rayDir * t;
}
void RTSClient::Update(float DeltaTime)
{
	InputLogic();
	World::Get().UpdateAllTransforms();
	World::Get().UpdateAll(DeltaTime);	// pass deltaTime to world update
}
void RTSClient::FixedUpdate(float FixedStep)
{
	auto& CommandBus = AtlasNetClient::Get().GetCommandBus();

	PlayerCameraMoveCommand cameraMoveCommand;
	cameraMoveCommand.NewCameraLocation = cameraPivot->transform.Pos;
	std::swap(cameraMoveCommand.NewCameraLocation.y, cameraMoveCommand.NewCameraLocation.z);
	CommandBus.Dispatch(cameraMoveCommand);

	CommandBus.Flush();
}
void RTSClient::OnPlayerAssignStateCommand(const NetServerStateHeader& header,
										   const PlayerAssignStateCommand& command)
{
	myAssignedTeam = command.yourTeam;
}
void RTSClient::InputLogic()
{
	if (ImGui::IsKeyReleased(ImGuiKey_P))
	{
		float PosRange = 100;
		// random location within -100/100 on xz plane

		std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
		WorkerMoveCommand moveCommand;

		for (const auto& wID : SelectedWorkers)
		{
			if (LocalID2RemoteID.contains(wID))
			{
				Worker& worker = World::Get().GetEntity<Worker>(wID);
				vec3 p = vec3(dist(rng), 0, dist(rng));
				moveCommand.Moves.push_back({LocalID2RemoteID.at(wID), p});
			}
		}
		AtlasNetClient::Get().GetCommandBus().Dispatch(moveCommand);
	}
	static vec2 aabbStart;
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		aabbStart = Window::Get().GetMousePosNDC();
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		if (!ImGui::IsKeyDown(ImGuiKey_LeftShift))
		{
			for (EntityID wID : SelectedWorkers)
			{
				Worker* worker = World::Get().TryGetEntity<Worker>(wID);
				if (worker)
				{
					worker->ClearColorOVerride();
				}
			}
			SelectedWorkers.clear();
		}
		vec2 aabbEnd = Window::Get().GetMousePosNDC();
		if (aabbStart.x > aabbEnd.x)
			std::swap(aabbStart.x, aabbEnd.x);
		if (aabbStart.y > aabbEnd.y)
			std::swap(aabbStart.y, aabbEnd.y);
		std::vector<glm::vec3> frustumCorners;
		glm::mat4 VP = camera->GetViewProjMat();
		glm::mat4 invVP = glm::inverse(VP);

		for (float z : {-1.0f, 1.0f})  // near/far in NDC
		{
			for (float x : {aabbStart.x, aabbEnd.x})
				for (float y : {aabbStart.y, aabbEnd.y})
				{
					glm::vec4 p = invVP * glm::vec4(x, y, z, 1.0f);
					frustumCorners.push_back(glm::vec3(p));
				}
		}
		Box3f selectionBox;
		for (const auto& corner : frustumCorners)
		{
			bg::expand(selectionBox,
					   bg::model::point<float, 3, bg::cs::cartesian>(corner.x, corner.y, corner.z));
		}
		std::vector<EntityID> Candidates;
		std::vector<Value3f> results;
		World::Get().GetAABBTree().query(bgi::intersects(selectionBox),
										 std::back_inserter(results));
		Candidates.reserve(results.size());
		for (const auto& [box, id] : results)
		{
			Candidates.push_back(id);
		}
		for (EntityID ID : Candidates)
		{
			vec3 pos = World::Get().GetEntity<Entity>(ID).transform.Pos;
			vec4 NDCPos = VP * vec4(pos, 1.0f);
			NDCPos /= NDCPos.w;
			if (NDCPos.x >= std::min(aabbStart.x, aabbEnd.x) &&
				NDCPos.x <= std::max(aabbStart.x, aabbEnd.x) &&
				NDCPos.y >= std::min(aabbStart.y, aabbEnd.y) &&
				NDCPos.y <= std::max(aabbStart.y, aabbEnd.y))
			{
				Worker* worker = World::Get().TryGetEntity<Worker>(ID);
				if (worker)

				{
					SelectedWorkers.insert(ID);
					worker->SetColorOverride(vec3(0, 1, 0));
				}
			}
		}
	}

	const vec2 mouseNDC = Window::Get().GetMousePosNDC();
	static std::vector<vec3> EndWorkerPositions;
	// --- RIGHT CLICK: start drag ---
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		RightClickStartNDC = mouseNDC;
		bRightClickDragging = true;
	}

	// --- RIGHT DRAG ---
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
	{
		bRightClickDragging = true;

		glm::vec3 startWorld = NDCToWorldXZ(RightClickStartNDC, camera->GetViewProjMat());
		glm::vec3 endWorld = NDCToWorldXZ(mouseNDC, camera->GetViewProjMat());

		// Draw drag line in world space
		DebugDraw::Get().DrawLine(startWorld, endWorld, vec3(1, 0, 0), false);
		EndWorkerPositions.resize(SelectedWorkers.size());
		std::cout << "Dragging from " << startWorld.x << "," << startWorld.z << " to " << endWorld.x
				  << "," << endWorld.z << "\n";
		for (int i = 0; i < EndWorkerPositions.size(); i++)
		{
			vec3 WorkerEndPos =
				EndWorkerPositions.size() == 1
					? startWorld
					: glm::mix(startWorld, endWorld, (float)i / (EndWorkerPositions.size() - 1));
			float boxSize = 0.1;
			DebugDraw::Get().DrawBox(WorkerEndPos - vec3(boxSize), WorkerEndPos + vec3(boxSize),
									 vec3(0, 1, 0), false);
			EndWorkerPositions[i] = WorkerEndPos;
		}
	}

	// --- RIGHT RELEASE: decide action ---
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		if (bRightClickDragging)
		{
			glm::vec3 startWorld = NDCToWorldXZ(RightClickStartNDC, camera->GetViewProjMat());
			glm::vec3 endWorld = NDCToWorldXZ(mouseNDC, camera->GetViewProjMat());
			WorkerMoveCommand moveCommand;
			// Single click → move all selected workers to this point
			if (EndWorkerPositions.empty())
			{
				for (auto wID : SelectedWorkers)
				{
					if (LocalID2RemoteID.contains(wID))
					{
						Worker& worker = World::Get().GetEntity<Worker>(wID);
						moveCommand.Moves.push_back({LocalID2RemoteID.at(wID), endWorld});
					}
				}
			}
			else
			{
				int i = 0;
				for (auto wID : SelectedWorkers)
				{
					Worker& worker = World::Get().GetEntity<Worker>(wID);
					if (LocalID2RemoteID.contains(worker.GetID()))
					{
						moveCommand.Moves.push_back(
							{LocalID2RemoteID.at(worker.GetID()), EndWorkerPositions[i]});
					}
					i++;
				}
			}
			AtlasNetClient::Get().GetCommandBus().Dispatch(moveCommand);
		}
		bRightClickDragging = false;
	}
}
void RTSClient::RenderScreenText(const std::string_view text, vec4 color)
{
	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	// Choose a large font scale
	float fontSize = 48.0f;

	// Calculate text size
	ImVec2 textSize = ImGui::CalcTextSize(text.data());

	const auto& IO = ImGui::GetIO();
	// Center position

	// I want to render at top left corner of window
	ImVec2 pos = ImVec2(textSize.x * 0.5f, textSize.y * 0.5f) +
				 ImVec2(10, 10);  // 10 pixels padding from top-left corner
	ivec4 intColor = glm::floor(color * 255.0f);
	// Optional: shadow for readability
	drawList->AddText(nullptr, fontSize, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 255),
					  text.data());

	// Main text
	drawList->AddText(nullptr, fontSize, pos,
					  IM_COL32(intColor.r, intColor.g, intColor.b, intColor.a), text.data());
	Window::Get().PostRender();
}
void RTSClient::onGameStateCommand(const NetServerStateHeader& header,
								   const GameStateCommand& command)
{
	for (const auto& worker : command.Workers)
	{
		vec3 worldPos = worker.Position;
		std::swap(worldPos.y, worldPos.z);
		float WorkerSize = 1.0f;
		// DebugDraw::Get().DrawBox(worker.Position - vec3(WorkerSize),
		//						 worker.Position + vec3(WorkerSize),
		//						 PlayerTeamsToColor(worker.team), false);

		WorkersToParse.push_back(worker);
	}
}

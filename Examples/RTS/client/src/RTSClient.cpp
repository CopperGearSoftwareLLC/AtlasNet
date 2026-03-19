#include "RTSClient.hpp"

#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <thread>

#include "AtlasNetClient.hpp"
#include "Debug/DebugDraw.hpp"
#include "Entities/CameraPivot.hpp"
#include "Entities/DummyEntity.hpp"
#include "Entities/FloorEntity.hpp"
#include "Entities/WorkerEntity.hpp"
#include "Entity.hpp"
#include "GLFW/glfw3.h"
#include "Global/pch.hpp"
#include "Window.hpp"
#include "World.hpp"
#include "commands/ClientCameraMoveCommand.hpp"
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

	Window::Get().PreRender();
	camera = &World::Get().CreateEntity<Camera>();
	camera->SetPerspective(false);
	camera->transform.Pos = {0, 0, 200};

	ImDrawList* drawList = ImGui::GetForegroundDrawList();

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	const char* text = "Connecting to AtlasNet";

	// Choose a large font scale
	float fontSize = 48.0f;

	// Calculate text size
	ImVec2 textSize = ImGui::CalcTextSize(text);

	// Center position
	ImVec2 pos = ImVec2((displaySize.x - textSize.x) * 0.5f, (displaySize.y - textSize.y) * 0.5f);

	// Optional: shadow for readability
	drawList->AddText(nullptr, fontSize, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 255),
					  text);

	// Main text
	drawList->AddText(nullptr, fontSize, pos, IM_COL32(255, 255, 255, 255), text);
	Window::Get().PostRender();
	glfwPollEvents();

	AtlasNetClient::InitializeProperties properties;
	logger.DebugFormatted("Connecting To AtlasNet at {}", address.ToString());
	properties.AtlasNetProxyIP = address;
	AtlasNetClient::Get().Initialize(properties);

	auto& CameraDummy = World::Get().CreateEntity<CameraPivot>();
	CameraDummy.Own(camera->GetID());
	CameraDummy.transform.Rot = quat(radians(vec3(-35, -45, 0)));
	cameraPivot = &CameraDummy;
	std::mt19937 rng(std::random_device{}());
	std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

	for (int i = 0; i < 10; i++)
	{
		auto& worker = World::Get().CreateEntity<Worker>();
		worker.transform.Pos = vec3(dist(rng), 0.0f, dist(rng));
	}
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
	}
}

void RTSClient::Render(float deltaTime)
{
	World::Get().DebugMenu();
	World::Get().RenderAll();

	DebugDraw::Get().Flush(camera->GetViewProjMat());
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
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const auto HitEntID =
			World::Get().Raycast(Window::Get().GetMousePosNDC(), camera->GetViewProjMat());
		if (HitEntID.has_value())
		{
			Worker& worker = World::Get().GetEntity<Worker>(*HitEntID);
			worker.SetColorOverride(WorkerSelectedColor);
			if (!ImGui::IsKeyDown(ImGuiKey_LeftShift))
			{
				for (const auto& wID : SelectedWorkers)
				{
					World::Get().GetEntity<Worker>(wID).ClearColorOVerride();
				}
				SelectedWorkers.clear();
			}
			SelectedWorkers.insert(worker.GetID());
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

			// Single click → move all selected workers to this point
			if (EndWorkerPositions.empty())
			{
				for (auto wID : SelectedWorkers)
				{
					Worker& worker = World::Get().GetEntity<Worker>(wID);
					worker.MoveTo(endWorld);  // implement MoveToXZ for world pos
				}
			}
			else
			{
				int i = 0;
				for (auto wID : SelectedWorkers)
				{
					Worker& worker = World::Get().GetEntity<Worker>(wID);
					worker.MoveTo(EndWorkerPositions[i]);  // implement MoveToXZ for world pos
					i++;
				}
			}
		}
		bRightClickDragging = false;
	}
	World::Get().UpdateAllTransforms();
	World::Get().UpdateAll(DeltaTime);	// pass deltaTime to world update
}
void RTSClient::FixedUpdate(float FixedStep)
{
	auto& CommandBus = AtlasNetClient::Get().GetCommandBus();
	
	ClientCameraMoveCommand cameraMoveCommand;
	cameraMoveCommand.NewCameraLocation = cameraPivot->transform.Pos;
	std::swap(cameraMoveCommand.NewCameraLocation.y, cameraMoveCommand.NewCameraLocation.z);
	CommandBus.Dispatch(cameraMoveCommand);

	CommandBus.Flush();
}

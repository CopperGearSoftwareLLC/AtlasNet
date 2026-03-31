#include "CameraPivot.hpp"

#include "Entities/Camera.hpp"
#include "Global/pch.hpp"
#include "Window.hpp"
#include "World.hpp"
#include "imgui.h"
void CameraPivot::Update(float deltaTime)
{
	auto& window = Window::Get();
	glm::vec2 mouseNDC = window.GetMousePosNDC();  // x,y in [-1,1]
	float angle = glm::radians(60.0f * deltaTime);

	int RotDir = (ImGui::IsKeyDown(ImGuiKey_Q) ? -1 : 0) + (ImGui::IsKeyDown(ImGuiKey_E) ? 1 : 0);

	if (RotDir != 0)
	{
		glm::quat rotQuat = glm::angleAxis(angle * RotDir, glm::vec3(0, 1, 0));

		// 🔥 WORLD rotation (this is the key change)
		transform.Rot = rotQuat * transform.Rot;
	}
	// Right mouse button check
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		if (!bDragging)
		{
			bDragging = true;
			lastMouseNDC = mouseNDC;
		}
		else
		{
			glm::vec2 deltaNDC = mouseNDC - lastMouseNDC;
			lastMouseNDC = mouseNDC;

			if (auto camID = Camera::GetMainCameraID(); camID.has_value())
			{
				auto& cam = World::Get().GetEntity<Camera>(camID.value());

				// Only for orthographic cameras

				float aspect = Window::Get().GetWindowAspect();
				float worldHalfWidth = cam.GetOrthoSensorSize() * aspect;
				float worldHalfHeight = cam.GetOrthoSensorSize();

				glm::mat4 view = cam.GetViewMat();

				// Camera axes in world space
				glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
				glm::vec3 camUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
				glm::vec3 camForward = glm::normalize(
					glm::vec3(-view[0][2], -view[1][2], -view[2][2]));	// OpenGL convention

				// Apply drag in camera plane
				glm::vec3 move =
					-deltaNDC.x * worldHalfWidth * camRight - deltaNDC.y * worldHalfHeight * camUp;

				transform.Pos += move;

				// Project onto XZ plane along camera forward direction
				// Compute the Y offset along camera forward
				float t = (0.0f - transform.Pos.y) / camForward.y;	// intersect Y=0 plane
				transform.Pos += camForward * t;

				// Optional: keep Y exactly zero
				transform.Pos.y = 0.0f;
			}
		}
	}
	else
	{
		bDragging = false;
	}
	// --- KEYBOARD MOVEMENT (WASD) ---
	if (auto camID = Camera::GetMainCameraID(); camID.has_value())
	{
		auto& cam = World::Get().GetEntity<Camera>(camID.value());

		glm::mat4 view = cam.GetViewMat();

		// Camera axes in world space
		glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
		glm::vec3 camUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
		glm::vec3 camForward = glm::normalize(glm::vec3(-view[0][2], -view[1][2], -view[2][2]));

		camForward.y = 0.0f;
		camForward = glm::normalize(camForward);

		camRight.y = 0.0f;
		camRight = glm::normalize(camRight);

		float aspect = Window::Get().GetWindowAspect();
		float worldHalfWidth = cam.GetOrthoSensorSize();
		float worldHalfHeight = cam.GetOrthoSensorSize();

		// Base speed proportional to visible world size
		float speed = worldHalfHeight * 2.0f;  // tweak multiplier if needed

		glm::vec3 move(0.0f);

		if (ImGui::IsKeyDown(ImGuiKey_W))
			move += camForward;
		if (ImGui::IsKeyDown(ImGuiKey_S))
			move -= camForward;
		if (ImGui::IsKeyDown(ImGuiKey_D))
			move += camRight;
		if (ImGui::IsKeyDown(ImGuiKey_A))
			move -= camRight;

		if (glm::length(move) > 0.0f)
		{
			move = glm::normalize(move) * speed * deltaTime;
			transform.Pos += move;

			// Keep on XZ plane
			transform.Pos.y = 0.0f;
		}
	}
}

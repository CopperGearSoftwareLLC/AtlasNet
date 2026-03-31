#pragma once

#include <imgui.h>

#include <glm/common.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <optional>

#include "Entity.hpp"
#include "EntityID.hpp"
#include "Global/pch.hpp"

class Camera : public Entity
{
	static inline std::optional<EntityID> MainCamera;

	glm::mat4 ViewMat, ProjMat, ViewProjMat;

	// Camera properties
	float SensorScaleHalf = 10.0f, TargetSensorHalf = SensorScaleHalf;	// for ortho

	float FOV = 60.0f;	// perspective vertical FOV
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;

	bool bIsPerspective = true;	 // true = perspective, false = orthographic

   public:
	Camera(EntityID _ID) : Entity(_ID)
	{
		if (!MainCamera.has_value())
		{
			MainCamera = _ID;
		}
	}

	[[nodiscard]] static std::optional<EntityID> GetMainCameraID() { return MainCamera; }

	void Start() override {}
	void Update(float deltaTime) override
	{
		SensorScaleHalf = std::lerp(SensorScaleHalf, TargetSensorHalf, deltaTime * 25);
		UpdateMatrices();
		TargetSensorHalf -= ImGui::GetIO().MouseWheel * 2;
		TargetSensorHalf = std::clamp(TargetSensorHalf, 1.0f, 100.0f);
	}
#ifdef RTS_CLIENT
	void Render() override {}
#endif
	void End() override {}

	[[nodiscard]] const glm::mat4& GetViewMat() const { return ViewMat; }
	[[nodiscard]] const glm::mat4& GetProjMat() const { return ProjMat; }
	[[nodiscard]] const glm::mat4& GetViewProjMat() const { return ViewProjMat; }

	void SetPerspective(bool enable)
	{
		bIsPerspective = enable;
		UpdateMatrices();
	}

	float GetOrthoSensorSize() const { return SensorScaleHalf; }
#ifdef RTS_CLIENT
	void DebugMenu() override
	{
		if (ImGui::TreeNode("Camera"))
		{
			ImGui::Checkbox("Perspective", &bIsPerspective);
			ImGui::DragFloat("Near Plane", &NearPlane, 0.01f, 0.01f, 1000.0f);
			ImGui::DragFloat("Far Plane", &FarPlane, 0.1f, 0.1f, 10000.0f);

			if (bIsPerspective)
			{
				ImGui::DragFloat("FOV", &FOV, 0.1f, 1.0f, 179.0f);
			}
			else
			{
				ImGui::DragFloat("Ortho Half Size", &SensorScaleHalf, 0.1f, 0.1f, 1000.0f);
			}

			if (ImGui::Button("Reset Camera"))
			{
				SensorScaleHalf = 10.0f;
				FOV = 60.0f;
				NearPlane = 0.1f;
				FarPlane = 1000.0f;
			}
			ImGui::TreePop();
		}
	}
#endif

   private:
	void UpdateMatrices()
	{
		// Build view matrix from transform

		ViewMat = glm::inverse(GetWorldMatrix());

		// Projection matrix
		float aspect = 16.0f / 9.0f;  // replace with actual viewport aspect ratio
		if (bIsPerspective)
		{
			ProjMat = glm::perspective(glm::radians(FOV), aspect, NearPlane, FarPlane);
		}
		else
		{
			ProjMat = glm::ortho(-SensorScaleHalf * aspect, SensorScaleHalf * aspect,
								 -SensorScaleHalf, SensorScaleHalf, NearPlane, FarPlane);
		}

		ViewProjMat = ProjMat * ViewMat;
	}
};
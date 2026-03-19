#pragma once

#include "Global/pch.hpp"
#ifdef RTS_CLIENT
#include "imgui.h"
#endif
class Transform
{
   public:
	vec3 Pos = {0, 0, 0};
	quat Rot = quat(vec3(0));
	vec3 Scale = {1, 1, 1};

	[[nodiscard]] mat4 GetModelMatrix() const
	{
		// Convert quaternion to rotation matrix (3x3)
		mat3 R = mat3_cast(Rot);

		// Apply scale directly to rotation columns (R * S)
		R[0] *= Scale.x;
		R[1] *= Scale.y;
		R[2] *= Scale.z;

		// Construct final mat4
		mat4 M(1.0f);
		M[0] = vec4(R[0], 0.0f);
		M[1] = vec4(R[1], 0.0f);
		M[2] = vec4(R[2], 0.0f);
		M[3] = vec4(Pos, 1.0f);

		return M;
	}
	[[nodiscard]] mat4 GetViewMatrix() const
	{
		// Inverse of TRS

		// Inverse scale
		vec3 invScale = 1.0f / Scale;

		// Inverse rotation (transpose for unit quaternion)
		mat3 R = mat3_cast(Rot);
		mat3 Rt = transpose(R);

		// Combine S^-1 * R^T
		mat3 invRS = Rt;
		invRS[0] *= invScale.x;
		invRS[1] *= invScale.y;
		invRS[2] *= invScale.z;

		// Inverse translation
		vec3 invT = -(invRS * Pos);

		// Build matrix
		mat4 V(1.0f);
		V[0] = vec4(invRS[0], 0.0f);
		V[1] = vec4(invRS[1], 0.0f);
		V[2] = vec4(invRS[2], 0.0f);
		V[3] = vec4(invT, 1.0f);

		return V;
	}
#ifdef RTS_CLIENT
	void DebugMenu()
	{
		if (ImGui::TreeNode("Transform"))
		{
			// Position
			ImGui::DragFloat3("Position", &Pos.x, 0.1f);

			// Scale
			ImGui::DragFloat3("Scale", &Scale.x, 0.01f, 0.0f, 0.0f, "%.3f");

			// Convert quaternion to Euler angles (degrees)
			vec3 euler = glm::degrees(glm::eulerAngles(Rot));
			vec3 newEuler = euler;

			if (ImGui::DragFloat3("Rotation (Euler)", &newEuler.x, 0.5f, -360.0f, 360.0f))
			{
				// Convert back to quaternion
				Rot = glm::quat(glm::radians(newEuler));
			}

			// Optional: Display the quaternion itself
			ImGui::Text("Quaternion: (%.3f, %.3f, %.3f, %.3f)", Rot.x, Rot.y, Rot.z, Rot.w);
			ImGui::TreePop();
		}
	}
#endif
};
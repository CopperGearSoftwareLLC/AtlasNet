#pragma once

#include <tweeny/easing.h>
#include <tweeny/tween.h>

#include <glm/ext/quaternion_geometric.hpp>

#include "Client/Client.hpp"
#include "Entity.hpp"
#include "EntityID.hpp"
	#ifdef RTS_CLIENT

#include "GL/ElementBuffer.hpp"
#include "GL/VertexArray.hpp"
#include "GL/VertexBuffer.hpp"
#include "GL/shader.hpp"
#endif
#include "Global/pch.hpp"
#include "tweeny/tweeny.h"
class Worker : public Entity
{
	#ifdef RTS_CLIENT

	constexpr static const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
out float yPos;
uniform mat4 MVP;

void main()
{
    gl_Position = MVP * vec4(aPos, 1.0);
    yPos = aPos.y-1;
}
)";

	constexpr static const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
in float yPos;
uniform vec3 uColor; // external color

void main()
{
    float gradient = (1 - clamp(yPos + 1.0, 0, 1));
    float powGradient = 1 - (gradient * gradient);
    vec3 color = uColor * mix(0.4, 1.0, powGradient);
    FragColor = vec4(color, 1.0);
}
)";

	std::vector<glm::vec3> cubeVertices = {
		{-1, 0, -1}, {1, 0, -1}, {1, 2, -1}, {-1, 2, -1},  // back
		{-1, 0, 1},	 {1, 0, 1},	 {1, 2, 1},	 {-1, 2, 1}	   // front
	};

	std::vector<unsigned int> cubeIndices = {// back face
											 0, 1, 2, 2, 3, 0,
											 // front face
											 4, 5, 6, 6, 7, 4,
											 // left
											 0, 3, 7, 7, 4, 0,
											 // right
											 1, 2, 6, 6, 5, 1,
											 // bottom
											 0, 1, 5, 5, 4, 0,
											 // top
											 3, 2, 6, 6, 7, 3};
	VertexArray vao;
	VertexBuffer vbo;
	ElementBuffer ebo;
	ShaderProgram shader;
	#endif
	ClientID OwnerID;
	glm::vec3 color = glm::vec3(1.0f, 0.7f, 0.2f);	// default color
	std::optional<vec3> ColorOverride;
	float MoveSpeed = 0.05;
	tweeny::tween<float, float, float> movementTween;
	float TweenStepPerSecond;

   public:
	Worker(EntityID _ID);
	void Start() override {}
	void Update(float deltaTime) override
	{
		if (!movementTween.isFinished())
		{
			movementTween.step(deltaTime *
							   TweenStepPerSecond);	 // advance proportional to deltaTime
		}
	}
	#ifdef RTS_CLIENT
	void Render() override;
	void SetColorOverride(const glm::vec3& newColor) { ColorOverride = newColor; }
	void ClearColorOVerride() { ColorOverride.reset(); }
	#endif
	void End() override {}
	void MoveTo(vec3 endWorldPos)
	{
		float len = glm::length(transform.Pos - endWorldPos);
		float durationSec = len / MoveSpeed;  // seconds
		int steps = 240;					  // number of steps (you can adjust for smoothness)

		movementTween = tweeny::from(transform.Pos.x, transform.Pos.y, transform.Pos.z)
							.to(endWorldPos.x, endWorldPos.y, endWorldPos.z)
							.during(durationSec)  // duration in seconds, not steps
							.via(tweeny::easing::sinusoidalInEasing())
							.onStep(
								[this](float x, float y, float z)
								{
									transform.Pos = vec3(x, y, z);
									return false;
								});
		TweenStepPerSecond = static_cast<float>(steps) / durationSec;
	}
};
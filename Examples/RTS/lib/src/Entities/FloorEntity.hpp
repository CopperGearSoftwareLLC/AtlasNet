#pragma once

#include "Entity.hpp"
#include "EntityID.hpp"
#ifdef RTS_CLIENT
#include "GL/ElementBuffer.hpp"
#include "GL/VertexArray.hpp"
#include "GL/VertexBuffer.hpp"
#include "GL/shader.hpp"
#endif
class FloorEntity : public Entity
{
#ifdef RTS_CLIENT
	constexpr static const char* vertexShaderSrc = R"(
		#version 330 core
		layout (location = 0) in vec3 aPos;
		
		out vec2 vUV;
		
		uniform mat4 MVP;
		uniform mat4 ModelMat;
		
		void main()
		{
			vUV = (ModelMat*vec4(aPos,1.0)).xz * 0.5 + 0.5; // map from [-1,1] -> [0,1]
			gl_Position = MVP * vec4(aPos, 1.0);
}
)";

	constexpr static const char* fragmentShaderSrc = R"(
	#version 330 core
	in vec2 vUV;
	out vec4 FragColor;

	void main()
{
    // Compute checker pattern
    float x = floor(vUV.x);
    float y = floor(vUV.y);
    float checker = mod(x + y, 2.0);
	
    vec3 color = mix(vec3(0.25), vec3(0.30), checker); // white and black
    FragColor = vec4(color, 1.0);
	}
	)";
	std::vector<glm::vec3> quadVertices = {
		{
			-1.0f,
			0.0f,
			-1.0f,
		},	// bottom-left
		{
			1.0f,
			0.0f,
			-1.0f,
		},	// bottom-right
		{
			1.0f,
			0.0f,
			1.0f,
		},	// top-right
		{
			-1.0f,
			0.0f,
			1.0f,
		}  // top-left
	};

	std::vector<unsigned int> quadIndices = {0, 1, 2, 2, 3, 0};

	VertexArray vao;
	VertexBuffer vbo;
	ElementBuffer ebo;
	ShaderProgram shader;
#endif
	void Start() override {}
	void Update(float deltaTime) override {}
#ifdef RTS_CLIENT
	void Render() override;
#endif
	void End() override {}

   public:
	FloorEntity(EntityID _ID);
};
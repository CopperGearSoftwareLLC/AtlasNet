#pragma once

#include <vector>

#include "GL/VertexArray.hpp"
#include "GL/VertexBuffer.hpp"
#include "GL/shader.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/pch.hpp"
class DebugDraw : public Singleton<DebugDraw>
{
   public:
	struct Line
	{
		vec3 start;
		vec3 end;
		vec3 color;
		bool depthTest = true;
	};

	struct Box
	{
		vec3 min;
		vec3 max;
		vec3 color;
		bool depthTest = true;
	};

   private:
	std::vector<Line> lines;
	std::vector<Box> boxes;

	VertexArray vao;
	VertexBuffer vbo;
	ShaderProgram shader;

   public:
	DebugDraw() : vbo(0, GL_DYNAMIC_DRAW)
	{
		// Create shader
		const char* vs = R"(
        #version 430 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() { gl_Position = uMVP * vec4(aPos,1.0); }
        )";

		const char* fs = R"(
        #version 430 core
        out vec4 FragColor;
        uniform vec3 uColor;
        void main() { FragColor = vec4(uColor,1.0); }
        )";

		shader.AddShader(vs, GL_VERTEX_SHADER);
		shader.AddShader(fs, GL_FRAGMENT_SHADER);
		shader.LinkProgram();
		// Create VAO/VBO
	}

	~DebugDraw() {}

	void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color,
				  bool depthTest = true)
	{
		lines.push_back({start, end, color, depthTest});
	}

	void DrawBox(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color,
				 bool depthTest = true)
	{
		boxes.push_back({min, max, color, depthTest});
	}

	void Flush(const glm::mat4& viewProj)
	{
		vao.Bind();
		shader.UseShader();

		for (const auto& line : lines)
		{
			if (line.depthTest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			glm::vec3 verts[2] = {line.start, line.end};
			vbo.Bind();
			vbo.SetData(verts, sizeof(verts));

			vao.EnableAttribute(0);
			vao.SetAttribute(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

			shader.SetMat4("uMVP", viewProj);
			shader.SetVec3("uColor", line.color);
			glDrawArrays(GL_LINES, 0, 2);
		}

		for (const auto& box : boxes)
		{
			if (box.depthTest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			glm::vec3 corners[8] = {
				{box.min.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.min.z},
				{box.max.x, box.max.y, box.min.z}, {box.min.x, box.max.y, box.min.z},
				{box.min.x, box.min.y, box.max.z}, {box.max.x, box.min.y, box.max.z},
				{box.max.x, box.max.y, box.max.z}, {box.min.x, box.max.y, box.max.z}};

			int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
								{6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

			for (int i = 0; i < 12; i++)
			{
				glm::vec3 verts[2] = {corners[edges[i][0]], corners[edges[i][1]]};
				vbo.SetData(verts, sizeof(verts));

				vao.EnableAttribute(0);
				vao.SetAttribute(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

				shader.SetMat4("uMVP", viewProj);
				shader.SetVec3("uColor", box.color);
				glDrawArrays(GL_LINES, 0, 2);
			}
		}

		lines.clear();
		boxes.clear();
	}
};
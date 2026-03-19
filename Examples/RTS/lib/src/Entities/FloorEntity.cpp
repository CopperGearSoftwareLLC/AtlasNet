#include "FloorEntity.hpp"
#ifdef RTS_CLIENT
#include "Entities/Camera.hpp"
#endif
#include "Global/pch.hpp"
#include "World.hpp"

FloorEntity::FloorEntity(EntityID _ID)
	: Entity(_ID)
#ifdef RTS_CLIENT
	  ,
	  vao(),
	  vbo(quadVertices.size() * sizeof(vec3), GL_STATIC_DRAW, quadVertices.data()),
	  ebo(quadIndices)
#endif
{
#ifdef RTS_CLIENT
	shader.AddShader(vertexShaderSrc, GL_VERTEX_SHADER);
	shader.AddShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
	shader.LinkProgram();
	vao.Bind();
	vbo.Bind();
	ebo.Bind();
	vao.EnableAttribute(0);
	vao.SetAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	vao.Unbind();
#endif
}
#ifdef RTS_CLIENT
void FloorEntity::Render()
{
	glEnable(GL_DEPTH_TEST);

	ASSERT(Camera::GetMainCameraID().has_value(), "NO CAMERA ASSIGNED");
	shader.UseShader();

	mat4 modelMat = GetWorldMatrix();
	shader.SetMat4("ModelMat", modelMat);
	shader.SetMat4(
		"MVP", World::Get().GetEntity<Camera>(Camera::GetMainCameraID().value()).GetViewProjMat() *
				   modelMat);
	vao.Bind();
	glDrawElements(GL_TRIANGLES, quadIndices.size(), GL_UNSIGNED_INT, nullptr);
	shader.UnuseShader();
	vao.Unbind();
}

#endif
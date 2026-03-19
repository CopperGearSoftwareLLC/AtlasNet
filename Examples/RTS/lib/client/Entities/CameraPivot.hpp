#pragma once

#include "Entity.hpp"
class CameraPivot : public Entity
{
	glm::vec2 lastMouseNDC = {0.0f, 0.0f};
	bool bDragging = false;

   public:
	CameraPivot(EntityID _ID) : Entity(_ID) {}
	void Start() override {}
	void Update(float deltaTime) override;

	void Render() override {}

	void End() override {}
};
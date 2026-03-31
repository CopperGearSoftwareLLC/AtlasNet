#pragma once

#include "Entity.hpp"
class DummyEntity : public Entity
{
   public:
	DummyEntity(EntityID _ID) : Entity(_ID) {}
	void Start() override {}
	void Update(float deltaTime) override {}
	void Render() override {}
	void End() override {}
};
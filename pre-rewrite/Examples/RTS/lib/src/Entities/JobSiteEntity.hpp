#pragma once

#include "Entity.hpp"
class JobSiteEntity : public Entity
{
	void Start() override {}
	void Update(float deltaTime) override {}
#ifdef RTS_CLIENT
	void Render() override {}
	void DebugMenu() override {}

#endif
	void End() override;
};
#pragma once

#include <vector>

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Entity/Entity.hpp"
#include "Global/pch.hpp"

SERVER_STATE_COMMAND_BEGIN(GameStateCommand)
{
	vec3 yourPosition;
	struct Entity
	{
		vec3 position;
		AtlasEntityID ID;
	};
	std::vector<Entity> entities;
	void Serialize(ByteWriter & bw) const override
	{
		bw.vec3(yourPosition);
		bw.u64(entities.size());
		for (const auto& e : entities)
		{
			bw.vec3(e.position);
			bw.uuid(e.ID);
		}
	}
	void Deserialize(ByteReader & br) override
	{
		yourPosition = br.vec3();
		entities.resize(br.u64());
		for (auto& e : entities)
		{
			e.position = br.vec3();
			e.ID = br.uuid();
		}
	}
};
SERVER_STATE_COMMAND_END(GameStateCommand)
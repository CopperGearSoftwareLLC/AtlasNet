#pragma once

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Global/pch.hpp"

SERVER_STATE_COMMAND_BEGIN(GameStateCommand)
{
	vec3 yourPosition;
	void Serialize(ByteWriter & bw) const override
	{
		bw.vec3(yourPosition);
	}
	void Deserialize(ByteReader & br) override
	{
		yourPosition = br.vec3();
	}
};
SERVER_STATE_COMMAND_END(GameStateCommand)
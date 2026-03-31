#pragma once

#include "Command/NetCommand.hpp"
#include "Command/CommandRegistry.hpp"
CLIENT_INTENT_COMMAND_BEGIN(PlayerCameraMoveCommand)
{
	vec3 NewCameraLocation;
	void Serialize(ByteWriter & bw) const override
	{
		bw.vec3(NewCameraLocation);
	}
	void Deserialize(ByteReader & br) override
	{
		NewCameraLocation = br.vec3();
	}
};
CLIENT_INTENT_COMMAND_END(PlayerCameraMoveCommand)

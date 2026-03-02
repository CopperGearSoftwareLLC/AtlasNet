#pragma once

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"

CLIENT_INTENT_COMMAND_BEGIN(GameClientInputCommand)
{
	vec3 myDesiredDestination;
	void Serialize(ByteWriter & bw) const override
	{
		bw.vec3(myDesiredDestination);
	}
	void Deserialize(ByteReader & br) override
	{
		myDesiredDestination = br.vec3();
	}
};
CLIENT_INTENT_COMMAND_END(GameClientInputCommand)

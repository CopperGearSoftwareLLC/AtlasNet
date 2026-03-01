#pragma once

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"

SERVER_STATE_COMMAND_BEGIN(GameStateCommand){

	void Serialize(ByteWriter & bw) const override{} void Deserialize(ByteReader & br) override{}};
SERVER_STATE_COMMAND_END(GameStateCommand)
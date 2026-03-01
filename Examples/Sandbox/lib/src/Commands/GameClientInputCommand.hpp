#pragma once

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"

CLIENT_INTENT_COMMAND_BEGIN(GameClientInputCommand){

	void Serialize(ByteWriter & bw) const override{} void Deserialize(ByteReader & br) override{}};
CLIENT_INTENT_COMMAND_END(GameClientInputCommand)


#pragma once

#include "Command/NetCommand.hpp"

#include "PlayerColors.hpp"
#include "Command/CommandRegistry.hpp"
SERVER_STATE_COMMAND_BEGIN(PlayerAssignStateCommand)
{
	PlayerTeams yourTeam;
	void Serialize(ByteWriter & bw) const override
	{
		bw.write_scalar(yourTeam);
	}
	void Deserialize(ByteReader & br) override
	{
		yourTeam = br.read_scalar<PlayerTeams>();
	}
};
SERVER_STATE_COMMAND_END(PlayerAssignStateCommand)
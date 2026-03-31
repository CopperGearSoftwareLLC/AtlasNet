#pragma once

#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "PlayerColors.hpp"

struct PlayerData
{
	PlayerTeams playerColor;

	void Serialize(ByteWriter& bw) const { bw.write_scalar<PlayerTeams>(playerColor); }

	void Deserialize(ByteReader& br) { playerColor = br.read_scalar<PlayerTeams>(); }
};
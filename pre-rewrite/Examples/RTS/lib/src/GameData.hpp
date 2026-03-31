#pragma once

#include "EntityID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "PlayerColors.hpp"
struct WorkerData
{
	vec3 Position;
	PlayerTeams team;
	EntityID ID;
	vec3 TargetPosition, InitialPos;
	float MoveProgress = 0.0f;
	bool InMotion = false;
	void Serialize(ByteWriter& bw) const
	{
		bw.vec3(Position);
		bw.write_scalar<PlayerTeams>(team);
		bw.uuid(ID);
		bw.vec3(TargetPosition);
		bw.vec3(InitialPos);
		bw.f32(MoveProgress);
		bw.i8(InMotion);
	}
	void Deserialize(ByteReader& br)
	{
		Position = br.vec3();
		team = br.read_scalar<PlayerTeams>();
		ID = br.uuid();
		TargetPosition = br.vec3();
		InitialPos = br.vec3();
		MoveProgress = br.f32();
		InMotion = br.i8();
	}
};
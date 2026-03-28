#pragma once

#include <boost/container/small_vector.hpp>

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "Entities/WorkerEntity.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
CLIENT_INTENT_COMMAND_BEGIN(WorkerMoveCommand)
{
	struct WorkerMove
	{
		EntityID WorkerID;
		vec3 TargetPos;
	};
	boost::container::small_vector<WorkerMove, 20> Moves;
	void Serialize(ByteWriter & serializer) const override
	{
		serializer.u64(Moves.size());
		for (const auto& move : Moves)
		{
			serializer.uuid(move.WorkerID);
			serializer.vec3(move.TargetPos);
		}
	}
	void Deserialize(ByteReader & deserializer) override
	{
		uint64_t moveCount;
		moveCount = deserializer.u64();
		Moves.clear();
		Moves.reserve(moveCount);
		for (uint64_t i = 0; i < moveCount; ++i)
		{
			WorkerMove move;
			move.WorkerID = deserializer.uuid();
			move.TargetPos = deserializer.vec3();
			Moves.push_back(move);
		}
	}
};
CLIENT_INTENT_COMMAND_END(WorkerMoveCommand);

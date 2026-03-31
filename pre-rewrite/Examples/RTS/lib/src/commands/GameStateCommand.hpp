#pragma once

#include "Command/CommandRegistry.hpp"
#include "Command/NetCommand.hpp"
#include "GameData.hpp"
SERVER_STATE_COMMAND_BEGIN(GameStateCommand)
{
	boost::container::small_vector<WorkerData, 10> Workers;

	void Serialize(ByteWriter & bw) const override
	{
		bw.u64(Workers.size());
		for (const auto& worker : Workers)
		{
			worker.Serialize(bw);
		}
	}
	void Deserialize(ByteReader & br) override
	{
		const auto workerCount = br.u64();
		Workers.clear();
		for (size_t i = 0; i < workerCount; i++)
		{
			WorkerData data;
			data.Deserialize(br);
			Workers.push_back(data);
		}
	}
};
SERVER_STATE_COMMAND_END(GameStateCommand);
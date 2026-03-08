#pragma once

#include <boost/describe/enum_to_string.hpp>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "Events/Events/Transfer/EntityHandoffTransferEvent.hpp"
#include "Events/GlobalEvents.hpp"
#include "Global/Misc/Singleton.hpp"

class TransferStateQueueView
{
	static constexpr std::size_t kMaxBufferedEvents = 8192;

	std::mutex queueMutex;
	std::deque<EntityHandoffTransferEvent> eventQueue;

   public:
	TransferStateQueueView()
	{
		GlobalEvents::Get().Subscribe<EntityHandoffTransferEvent>(
			[this](const EntityHandoffTransferEvent& event)
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				eventQueue.push_back(event);
				if (eventQueue.size() > kMaxBufferedEvents)
				{
					eventQueue.pop_front();
				}
			});
	}

	void DrainTransferStateQueue(std::vector<std::vector<std::string>>& outRows)
	{
		std::deque<EntityHandoffTransferEvent> drained;
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			if (eventQueue.empty())
			{
				outRows.clear();
				return;
			}
			drained.swap(eventQueue);
		}

		outRows.clear();
		outRows.reserve(drained.size());
		for (const auto& event : drained)
		{
			std::vector<std::string> row;
			row.reserve(6 + event.entityIds.size());
			row.push_back(event.transferId);
			row.push_back(event.fromId);
			row.push_back(event.toId);
			row.push_back(boost::describe::enum_to_string(event.stage, "eUnknown"));
			row.push_back(event.state);
			row.push_back(std::to_string(event.timestampMs));
			for (const auto& entityId : event.entityIds)
			{
				row.push_back(entityId);
			}
			outRows.push_back(std::move(row));
		}
	}
};

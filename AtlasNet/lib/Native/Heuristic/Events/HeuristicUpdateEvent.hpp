#pragma once

#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
class HeuristicUpdateEvent : public IEvent
{
	void Serialize(ByteWriter& bw) const override {}
	void Deserialize(ByteReader& br) override {}
};
ATLASNET_REGISTER_EVENT(HeuristicUpdateEvent);
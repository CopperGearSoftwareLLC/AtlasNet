#pragma once

#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
struct LogEvent : IEvent
{
	std::string message;
	void Serialize(ByteWriter& bw) const override  { bw.str(message); }
	void Deserialize(ByteReader& br) override { message = br.str(); }
};
ATLASNET_REGISTER_EVENT(LogEvent);
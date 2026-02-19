#pragma once
#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
class ClientConnectEvent : public IEvent
{
	void Serialize(ByteWriter& bw) override {};
	void Deserialize(ByteReader& br) override {}  
};
ATLASNET_REGISTER_EVENT(ClientConnectEvent, "ClientConnectEvent");
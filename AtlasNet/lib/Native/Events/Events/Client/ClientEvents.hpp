#pragma once
#include "Client/Client.hpp"
#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
#include "Network/NetworkIdentity.hpp"
struct ClientConnectEvent : public IEvent
{
	Client client;
	NetworkIdentity ConnectedProxy;
	void Serialize(ByteWriter& bw) const override
	{
		client.Serialize(bw);
		ConnectedProxy.Serialize(bw);
	};
	void Deserialize(ByteReader& br) override
	{
		client.Deserialize(br);
		ConnectedProxy.Deserialize(br);
	}
};
ATLASNET_REGISTER_EVENT(ClientConnectEvent, "ClientConnectEvent");
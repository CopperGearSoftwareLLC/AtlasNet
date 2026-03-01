#pragma once
#include "Client/Client.hpp"
#include "Entity/Transform.hpp"
#include "Events/EventRegistry.hpp"
#include "Events/Events/IEvent.hpp"
#include "Network/NetworkIdentity.hpp"
struct ClientHandshakeEvent : public IEvent
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
ATLASNET_REGISTER_EVENT(ClientHandshakeEvent);
struct ClientConnectEvent : public IEvent
{
	Client client;
	NetworkIdentity ConnectedProxy;
	NetworkIdentity ConnectedShard;
	Transform SpawnLocation;
	void Serialize(ByteWriter& bw) const override
	{
		client.Serialize(bw);
		ConnectedProxy.Serialize(bw);
		ConnectedShard.Serialize(bw);
		SpawnLocation.Serialize(bw);
	};
	void Deserialize(ByteReader& br) override
	{
		client.Deserialize(br);
		ConnectedProxy.Deserialize(br);
		ConnectedShard.Deserialize(br);
		SpawnLocation.Deserialize(br);
	}
};
ATLASNET_REGISTER_EVENT(ClientConnectEvent);
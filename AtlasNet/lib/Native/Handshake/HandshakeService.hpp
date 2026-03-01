#pragma once

#include "Client/Client.hpp"
#include "Client/Packet/ClientSpawnPacket.hpp"
#include "Debug/Log.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
class HandshakeService : public Singleton<HandshakeService>
{
	Log logger = Log("HandshakeService");

   public:
	enum class ClientVerifyStatus
	{
		eAccepted,
		eRejected,
		eError
	};
	struct ClientVerifyReply
	{
		ClientVerifyStatus status;
		Transform SpawnWorldLocation;
	};
	void OnClientConnect(const Client& c)
	{
		ClientVerifyReply r = VerifyClient(c);
		logger.DebugFormatted("Client {} Spawn Location {}", UUIDGen::ToString(c.ID),
							  r.SpawnWorldLocation.ToString());
		std::optional<NetworkIdentity> targetShard = DetermineShard(r.SpawnWorldLocation);
		logger.DebugFormatted(
			"Client {} Target Shard {}", UUIDGen::ToString(c.ID),
			targetShard.has_value() ? targetShard.value().ToString() : "UNABLE TO DETERMINE");
		ASSERT(targetShard.has_value(),
			   "Unable to find a shard for that position. TODO, Implement a GetClosestBound in "
			   "IHeuristic");

		ClientSpawnPacket csp;
		csp.stage = ClientSpawnPacket::eNotification;
		csp.SetAsNotification().incomingClients.push_back(
			ClientSpawnPacket::NotificationData::NewClientData{
				.client = c, .spawn_Location = r.SpawnWorldLocation});
		Interlink::Get().SendMessage(*targetShard, csp, NetworkMessageSendFlag::eReliableBatched);
	}
	[[nodiscard]] ClientVerifyReply VerifyClient(const Client& c)
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
		ClientVerifyReply reply;
		reply.status = ClientVerifyStatus::eAccepted;
		reply.SpawnWorldLocation.position.x = dist(gen);
		reply.SpawnWorldLocation.position.y = dist(gen);
		reply.SpawnWorldLocation.position.z = 0.0f;
		return reply;
	}
	[[nodiscard]] std::optional<NetworkIdentity> DetermineShard(const Transform& t)
	{
		return HeuristicManifest::Get().ShardFromPosition(t);
	}
};
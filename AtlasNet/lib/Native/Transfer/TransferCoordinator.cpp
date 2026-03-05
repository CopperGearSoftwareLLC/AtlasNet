#include "TransferCoordinator.hpp"

#include <algorithm>

#include "Client/Client.hpp"
#include "Client/Database/ClientManifest.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Transfer/Packet/ClientSwitchPacket.hpp"
#include "Transfer/Packet/ClientTransferPacket.hpp"
#include "Transfer/Packet/EntityTransferPacket.hpp"
#include "Transfer/TransferData.hpp"
#include "Transfer/TransferManifest.hpp"

void TransferCoordinator::ParseEntitiesForTargets()
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	const std::unique_ptr<IHeuristic> heuristic = HeuristicManifest::Get().PullHeuristic();
	std::unordered_map<IBounds::BoundsID, EntityTransferData> NewEntityTransfers;
	std::unordered_map<IBounds::BoundsID, ClientTransferData> NewClientTransfers;

	while (!EntitiesToParseForReceiver.empty())
	{
		AtlasEntityID entityID = EntitiesToParseForReceiver.top();
		EntitiesToParseForReceiver.pop();

		// If ID already exists in EntitiesInTransfer then entity was already scheduled to be
		// transfered
		if (EntitiesInTransfer.contains(entityID))
		{
			//	logger.ErrorFormatted("Entiy {} already marked as in transfer",
			//						  UUIDGen::ToString(entityID));
			continue;
		}
		std::optional<ClientID> clientID;
		std::optional<Transform> entityTransform = EntityLedger::Get()._ReadLock(
			[&]() -> std::optional<Transform>
			{
				if (EntityLedger::Get().ExistsEntity(entityID))
				{
					AtlasEntityMinimal entityData = EntityLedger::Get().GetEntityMinimal(entityID);
					if (entityData.IsClient)
					{
						clientID.emplace(entityData.Client_ID);
					}
					return entityData.transform;
				}
				return std::nullopt;
			});

		if (!entityTransform.has_value())
		{
			continue;
		}
		// Get the Bounds ID at the entity position
		const std::optional<IBounds::BoundsID> boundsID =
			heuristic->QueryPosition(entityTransform->position);

		// if not a bound, aka outside any bound then continue
		if (!boundsID.has_value())
		{
			//	logger.ErrorFormatted("Entiy {}, was not able to find a correspoinding bound",
			//						  UUIDGen::ToString(entityID));
			continue;
		}
		if (clientID.has_value())
		{
			NewClientTransfers[*boundsID].Clients.push_back({*clientID, entityID});
		}
		else
		{
			NewEntityTransfers[*boundsID].entityIDs.push_back(entityID);
		}
	}

	for (auto it = NewEntityTransfers.begin(); it != NewEntityTransfers.end();)
	{
		EntityTransferData& TransferData = it->second;
		// get the shard that owns that bound id
		const auto TargetShard = HeuristicManifest::Get().ShardFromBoundID(it->first);
		// if it does not exist then no shard owns this bound, or the bound is self, ignore this
		// transfer
		if (!TargetShard.has_value() || TargetShard.value() == NetworkCredentials::Get().GetID())
		{
			it = NewEntityTransfers.erase(it);	// erase returns the next valid iterator
			continue;
		}
		TransferData.ID = UUIDGen::Gen();
		TransferData.transferMode = TransferMode::eSending;
		TransferData.shard = *TargetShard;
		TransferData.stage = EntityTransferStage::eNone;
		EntityPreTransfers.push(TransferData);
		for (const AtlasEntityID EntityID : TransferData.entityIDs)
		{
			EntitiesInTransfer.insert(std::make_pair(EntityID, TransferData.ID));
		}
		TransferManifest::Get().PushTransferInfo(TransferData);
		logger.DebugFormatted(
			"Scheduled new Entity Transfer:\n- ID {}\n- {} entities\n- Target: {}",
			UUIDGen::ToString(TransferData.ID), TransferData.entityIDs.size(),
			TransferData.shard.ToString());

		++it;
	}
	for (auto it = NewClientTransfers.begin(); it != NewClientTransfers.end();)
	{
		ClientTransferData& TransferData = it->second;
		// get the shard that owns that bound id
		const auto TargetShard = HeuristicManifest::Get().ShardFromBoundID(it->first);
		// if it does not exist then no shard owns this bound, or the bound is self, ignore this
		// transfer
		if (!TargetShard.has_value() || TargetShard.value() == NetworkCredentials::Get().GetID())
		{
			it = NewClientTransfers.erase(it);	// erase returns the next valid iterator
			continue;
		}
		TransferData.ID = UUIDGen::Gen();
		TransferData.transferMode = TransferMode::eSending;
		TransferData.shard = *TargetShard;
		TransferData.stage = ClientTransferStage::eNone;
		ClientsPreTransfers.push(TransferData);
		for (const auto& [clientID, entityID] : TransferData.Clients)
		{
			EntitiesInTransfer.insert(std::make_pair(entityID, TransferData.ID));
			ClientsInTransfer.insert(std::make_pair(clientID, TransferData.ID));
		}
		// TransferManifest::Get().PushTransferInfo(TransferData);
		logger.DebugFormatted("Scheduled new Client Transfer:\n- ID {}\n- {} clients\n- Target: {}",
							  UUIDGen::ToString(TransferData.ID), TransferData.Clients.size(),
							  TransferData.shard.ToString());

		++it;
	}
}
void TransferCoordinator::TransferTick()
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	while (!EntityPreTransfers.empty())
	{
		const EntityTransferData PreTransfer = EntityPreTransfers.top();
		EntityPreTransfers.pop();
		EntityTransferPacket etPacket;
		etPacket.TransferID = PreTransfer.ID;
		etPacket.stage = EntityTransferStage::ePrepare;
		EntityTransferPacket::PrepareStageData& PacketData =
			etPacket.Data.emplace<EntityTransferPacket::PrepareStageData>();
		PacketData.entityIDs = PreTransfer.entityIDs;

		Interlink::Get().SendMessage(PreTransfer.shard, etPacket,
									 NetworkMessageSendFlag::eReliableNow);

		// project iterator from ID index into Stage index, then modify safely
		EntityTransferData newTransfer = PreTransfer;
		newTransfer.stage = EntityTransferStage::ePrepare;
		auto stageIt = EntityTransfers.insert({newTransfer.ID, newTransfer});

		logger.DebugFormatted("Advanced Entity Transfer {} -> ePrepare",
							  UUIDGen::ToString(newTransfer.ID));
	}

	while (!ClientsPreTransfers.empty())
	{
		const ClientTransferData PreTransfer = ClientsPreTransfers.top();
		ClientsPreTransfers.pop();
		ClientTransferPacket etPacket;
		etPacket.TransferID = PreTransfer.ID;
		etPacket.stage = ClientTransferStage::ePrepare;
		ClientTransferPacket::PrepareStageData& PacketData =
			etPacket.Data.emplace<ClientTransferPacket::PrepareStageData>();
		PacketData.clients = PreTransfer.Clients;

		Interlink::Get().SendMessage(PreTransfer.shard, etPacket,
									 NetworkMessageSendFlag::eReliableNow);

		// project iterator from ID index into Stage index, then modify safely
		ClientTransferData newTransfer = PreTransfer;
		newTransfer.stage = ClientTransferStage::ePrepare;
		auto stageIt = ClientTransfers.insert({newTransfer.ID, newTransfer});

		logger.DebugFormatted("Advanced Client Transfer {} -> ePrepare",
							  UUIDGen::ToString(newTransfer.ID));
	}
}
void TransferCoordinator::OnEntityTransferPacketArrival(const EntityTransferPacket& p,
														const PacketManager::PacketInfo& info)
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);
	logger.DebugFormatted("Entity Transfer update\n - ID:{}\n - Stage:{}",
						  UUIDGen::ToString(p.TransferID),
						  boost::describe::enum_to_string(p.stage, "INVALID"));
	TransferManifest::Get().UpdateEntityTransferStage(p.TransferID, p.stage);
	// Switch depending on the stage of the transfer
	switch (p.stage)
	{
		case EntityTransferStage::eNone:
			ASSERT(false, "This should never happen");
			break;
		case EntityTransferStage::ePrepare:
		{
			const EntityTransferPacket::PrepareStageData& predataData = p.GetAsPrepareStage();
			EntityTransferData data;
			data.entityIDs.resize(predataData.entityIDs.size());
			std::copy(predataData.entityIDs.begin(), predataData.entityIDs.end(),
					  data.entityIDs.begin());
			data.ID = p.TransferID;
			data.shard = info.sender;
			data.transferMode = TransferMode::eReceiving;
			data.stage = EntityTransferStage::eReady;
			EntityTransfers.insert({data.ID, data});

			EntityTransferPacket response;
			response.stage = EntityTransferStage::eReady;
			response.TransferID = p.TransferID;
			response.SetAsReadyStage();
			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);
			logger.DebugFormatted("Entity Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(data.ID),
								  boost::describe::enum_to_string(data.stage, "INVALID"));
		}
		break;
		case EntityTransferStage::eReady:
		{
			const EntityTransferPacket::ReadyStageData& readyData = p.GetAsReadyStage();
			ASSERT(EntityTransfers.contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");

			EntityTransferData& transferEntry = EntityTransfers.at(p.TransferID);
			transferEntry.stage = EntityTransferStage::eCommit;

			EntityTransferPacket response;
			response.stage = EntityTransferStage::eCommit;
			response.TransferID = p.TransferID;

			EntityTransferPacket::CommitStageData& commitData = response.SetAsCommitStage();
			commitData.entitySnapshots.reserve(transferEntry.entityIDs.size());
			for (const AtlasEntityID EntityID : transferEntry.entityIDs)
			{
				EntityTransferPacket::CommitStageData::Data d;
				if (!EntityLedger::Get().ExistsEntity(EntityID))
				{
					logger.WarningFormatted(
						"Entity {}  was scheduled for transfer {} but its gone?!?",
						UUIDGen::ToString(EntityID), UUIDGen::ToString(p.TransferID));
					continue;
				}
				d.Snapshot = EntityLedger::Get().GetAndEraseEntity(EntityID);
				commitData.entitySnapshots.push_back(d);
			}
			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);
			logger.DebugFormatted("Entity Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(p.TransferID),
								  boost::describe::enum_to_string(transferEntry.stage, "INVALID"));
		}
		break;
		case EntityTransferStage::eCommit:
		{
			const EntityTransferPacket::CommitStageData& commitData = p.GetAsCommitStage();
			ASSERT(EntityTransfers.contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");
			EntityTransferData& transferEntry = EntityTransfers.at(p.TransferID);
			transferEntry.stage = EntityTransferStage::eCommit;

			EntityTransferPacket response;
			response.stage = EntityTransferStage::eComplete;
			response.TransferID = p.TransferID;

			EntityTransferPacket::CompleteStageData& completeData = response.SetAsCompleteStage();

			for (const auto& Data : p.GetAsCommitStage().entitySnapshots)
			{
				EntityLedger::Get().AddEntity(Data.Snapshot);
			}

			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);
			logger.DebugFormatted("Entity Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(p.TransferID),
								  boost::describe::enum_to_string(response.stage, "INVALID"));
			EntityTransfers.erase(transferEntry.ID);
		}
		break;
		case EntityTransferStage::eComplete:
		{
			ASSERT(EntityTransfers.contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");
			const EntityTransferData& transferEntry = EntityTransfers.at(p.TransferID);
			for (const auto& eID : transferEntry.entityIDs)
			{
				EntitiesInTransfer.erase(eID);
			}
			EntityTransfers.erase(p.TransferID);
			logger.DebugFormatted("Entity Transfer Complete\n - ID:{}",
								  UUIDGen::ToString(p.TransferID));
			TransferManifest::Get().DeleteTransferInfo(p.TransferID);
		}
		break;
	}
}
void TransferCoordinator::OnClientTransferPacketArrival(const ClientTransferPacket& p,
														const PacketManager::PacketInfo& info)
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);
	logger.DebugFormatted("Got a client transfer packet ID {}", UUIDGen::ToString(p.TransferID));

	switch (p.stage)
	{
		case ClientTransferStage::eNone:
			ASSERT(false, "This should never happen");
		case ClientTransferStage::ePrepare:

		{
			ClientTransferPacket response;
			response.stage = ClientTransferStage::eReady;
			response.TransferID = p.TransferID;
			response.SetAsReadyStage();

			ClientTransferData newTransfer;
			newTransfer.ID = p.TransferID;
			newTransfer.shard = info.sender;
			newTransfer.stage = ClientTransferStage::eReady;
			newTransfer.transferMode = TransferMode::eReceiving;
			newTransfer.Clients = p.GetAsPrepareStage().clients;
			ClientTransfers.insert({newTransfer.ID, newTransfer});
			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);
			logger.DebugFormatted("Client Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(p.TransferID),
								  boost::describe::enum_to_string(newTransfer.stage, "INVALID"));
		}
		break;

		case ClientTransferStage::eReady:
		{
			ClientSwitchPacket switchpacket;
			switchpacket.TransferID = p.TransferID;
			switchpacket.stage = ClientSwitchStage::eRequestSwitch;
			auto& switchdata = switchpacket.SetAsRequestSwitchStage();

			ASSERT(ClientTransfers.contains(p.TransferID), "This should never happen");
			const std::optional<NetworkIdentity> Proxy = ClientManifest::Get().GetClientProxy(
				ClientTransfers.at(p.TransferID).Clients.front().first);
			ASSERT(Proxy.has_value(), "This should never happen");
			for (const auto& [clientID, entityID] : ClientTransfers.at(p.TransferID).Clients)
			{
				switchdata.clientIDs.push_back(clientID);
			}
			Interlink::Get().SendMessage(*Proxy, switchpacket,
										 NetworkMessageSendFlag::eReliableNow);

			logger.DebugFormatted("Client Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(p.TransferID),
								  boost::describe::enum_to_string(switchpacket.stage, "INVALID"));
		}
		break;

		case ClientTransferStage::eDrained:

		{
			for (const auto& entity : p.GetAsDrainedStage().clientPayloads)
			{
				EntityLedger::Get().AddEntity(entity);
			}
			ClientSwitchPacket activatePacket;
			activatePacket.TransferID = p.TransferID;
			activatePacket.stage = ClientSwitchStage::eActivate;
			activatePacket.SetAsActivateStage();
			ASSERT(ClientTransfers.contains(p.TransferID), "This should never happen");
			const std::optional<NetworkIdentity> Proxy = ClientManifest::Get().GetClientProxy(
				ClientTransfers.at(p.TransferID).Clients.front().first);
			ASSERT(Proxy.has_value(), "This should never happen");
			Interlink::Get().SendMessage(*Proxy, activatePacket,
										 NetworkMessageSendFlag::eReliableNow);

			logger.DebugFormatted("Client Transfer responding\n - ID:{}\n - Stage:{}",
								  UUIDGen::ToString(p.TransferID),
								  boost::describe::enum_to_string(activatePacket.stage, "INVALID"));
			ClientTransfers.erase(p.TransferID);
		}
		break;
	}
}
void TransferCoordinator::OnClientSwitchPacketArrival(const ClientSwitchPacket& p,
													  const PacketManager::PacketInfo& info)
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);
	switch (p.stage)
	{
		case ClientSwitchStage::eNone:
		case ClientSwitchStage::eRequestSwitch:	 // This one is a shard response to proxy, it should
												 // never be received
		case ClientSwitchStage::eActivate:		 // This one is a shard response to proxy, it should
												 // never be received
			ASSERT(false, "This should never happen");
			break;

		case ClientSwitchStage::eFreeze:
		{
			ClientTransferPacket response;
			response.stage = ClientTransferStage::eDrained;
			response.TransferID = p.TransferID;

			ClientTransferPacket::DrainedStageData& DrainData = response.SetAsDrainedStage();

			for (const auto& [clientID, entityID] : ClientTransfers.at(p.TransferID).Clients)
			{
				AtlasEntity entity = EntityLedger::Get().GetAndEraseEntity(entityID);
				DrainData.clientPayloads.push_back(entity);
				ClientsInTransfer.erase(clientID);
				EntitiesInTransfer.erase(entityID);
			}

			Interlink::Get().SendMessage(ClientTransfers.at(p.TransferID).shard, response,
										 NetworkMessageSendFlag::eReliableNow);
			ClientTransfers.erase(p.TransferID);
		}
		break;
	}
}
void TransferCoordinator::MarkEntitiesForTransfer(const std::span<AtlasEntityID> entities)
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	for (const auto& ID : entities) EntitiesToParseForReceiver.push(ID);
}
bool TransferCoordinator::IsEntityInTransfer(AtlasEntityID ID) const
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	return EntitiesInTransfer.contains(ID);
}
void TransferCoordinator::TransferThreadEntry(std::stop_token st)
{
	while (!st.stop_requested())
	{
		ParseEntitiesForTargets();
		TransferTick();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}
TransferCoordinator::TransferCoordinator()
	: EntityTransferPacketSubscription(
		  Interlink::Get().GetPacketManager().Subscribe<EntityTransferPacket>(
			  [this](const EntityTransferPacket& p, const PacketManager::PacketInfo& info)
			  { OnEntityTransferPacketArrival(p, info); })),
	  ClientTransferPacketSubscription(
		  Interlink::Get().GetPacketManager().Subscribe<ClientTransferPacket>(
			  [this](const ClientTransferPacket& p, const PacketManager::PacketInfo& info)
			  { OnClientTransferPacketArrival(p, info); })),
	  ClientSwitchRequestPacketSubscription(
		  Interlink::Get().GetPacketManager().Subscribe<ClientSwitchPacket>(
			  [this](const ClientSwitchPacket& p, const PacketManager::PacketInfo& info)
			  { OnClientSwitchPacketArrival(p, info); }))
{
	TransferThread = std::jthread([this](std::stop_token st) { TransferThreadEntry(st); });
};

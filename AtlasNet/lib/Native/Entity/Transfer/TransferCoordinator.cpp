#include "TransferCoordinator.hpp"

#include <algorithm>

#include "Entity/Entity.hpp"
#include "Entity/EntityEnums.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Entity/Transfer/TransferData.hpp"
#include "Entity/Transfer/TransferManifest.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"

void TransferCoordinator::ParseEntitiesForTargets()
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);

	const std::unique_ptr<IHeuristic> heuristic = HeuristicManifest::Get().PullHeuristic();
	std::unordered_map<IBounds::BoundsID, EntityTransferData> NewEntityTransfers;

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
		std::optional<Transform> entityTransform = EntityLedger::Get()._ReadLock(
			[&]() -> std::optional<Transform>
			{
				if (EntityLedger::Get().ExistsEntity(entityID) &&
					!EntityLedger::Get().IsEntityClient(entityID))
				{
					return EntityLedger::Get().GetEntityMinimal(entityID).transform;
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
		// logger.ErrorFormatted("Entiy {} scheduled for transfer to {}",
		// UUIDGen::ToString(entityID), 					  *boundsID);
		NewEntityTransfers[*boundsID].entityIDs.push_back(entityID);

		// heuristic->QueryPosition(vec3 p)
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
		EntityTransfers.insert(TransferData);
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
}
void TransferCoordinator::TransferTick()
{
	std::lock_guard<std::mutex> lock(EntityTransferMutex);
	if (!EntityTransfers.empty())
	{
		std::vector<TransferID> toAdvance;
		auto& StageView = EntityTransfers.get<TransferByStage>();
		const auto none_view = StageView.equal_range(EntityTransferStage::eNone);
		for (auto it = none_view.first; it != none_view.second; it++)
		{
			EntityTransferPacket etPacket;
			etPacket.TransferID = it->ID;
			etPacket.stage = EntityTransferStage::ePrepare;
			EntityTransferPacket::PrepareStageData& PacketData =
				etPacket.Data.emplace<EntityTransferPacket::PrepareStageData>();
			PacketData.entityIDs = it->entityIDs;

			Interlink::Get().SendMessage(it->shard, etPacket, NetworkMessageSendFlag::eReliableNow);
			toAdvance.push_back(it->ID);
		}
		auto& idView = EntityTransfers.get<TransferByID>();
		for (auto id : toAdvance)
		{
			auto it = idView.find(id);
			if (it == idView.end())
			{
				logger.WarningFormatted(
					"Transfer {} disappeared before stage update (will not advance).",
					UUIDGen::ToString(id));
				continue;
			}

			// project iterator from ID index into Stage index, then modify safely
			auto stageIt = EntityTransfers.project<TransferByStage>(it);
			StageView.modify(stageIt, [](EntityTransferData& edata)
							 { edata.stage = EntityTransferStage::ePrepare; });
			logger.DebugFormatted("Advanced Transfer {} -> ePrepare", UUIDGen::ToString(id));
		}
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
			EntityTransfers.insert(data);

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
			ASSERT(EntityTransfers.get<TransferByID>().contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");
			const auto TransferEntryIt = EntityTransfers.get<TransferByID>().find(p.TransferID);
			EntityTransfers.modify(TransferEntryIt, [](EntityTransferData& data)
								   { data.stage = EntityTransferStage::eCommit; });

			EntityTransferPacket response;
			response.stage = EntityTransferStage::eCommit;
			response.TransferID = p.TransferID;

			EntityTransferPacket::CommitStageData& commitData = response.SetAsCommitStage();
			commitData.entitySnapshots.reserve(TransferEntryIt->entityIDs.size());
			for (const AtlasEntityID EntityID : TransferEntryIt->entityIDs)
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
			logger.DebugFormatted(
				"Entity Transfer responding\n - ID:{}\n - Stage:{}",
				UUIDGen::ToString(p.TransferID),
				boost::describe::enum_to_string(TransferEntryIt->stage, "INVALID"));
		}
		break;
		case EntityTransferStage::eCommit:
		{
			const EntityTransferPacket::CommitStageData& commitData = p.GetAsCommitStage();
			ASSERT(EntityTransfers.get<TransferByID>().contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");
			const auto TransferEntryIt = EntityTransfers.get<TransferByID>().find(p.TransferID);
			EntityTransfers.modify(TransferEntryIt, [](EntityTransferData& data)
								   { data.stage = EntityTransferStage::eCommit; });

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
			EntityTransfers.erase(TransferEntryIt);
		}
		break;
		case EntityTransferStage::eComplete:
		{
			ASSERT(EntityTransfers.get<TransferByID>().contains(p.TransferID),
				   "I was responded to with a ready package but I dont have an internal record of "
				   "this transfer");
			const auto TransferEntryIt = EntityTransfers.get<TransferByID>().find(p.TransferID);
			for (const auto& eID : TransferEntryIt->entityIDs)
			{
				EntitiesInTransfer.erase(eID);
			}
			EntityTransfers.erase(EntityTransfers.get<TransferByID>().find(p.TransferID));
			logger.DebugFormatted("Entity Transfer Complete\n - ID:{}",
								  UUIDGen::ToString(p.TransferID));
			TransferManifest::Get().DeleteTransferInfo(p.TransferID);
		}
		break;
	}
}

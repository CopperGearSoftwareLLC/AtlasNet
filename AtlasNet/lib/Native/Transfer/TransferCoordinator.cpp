#include "TransferCoordinator.hpp"

#include <algorithm>
#include <mutex>
#include <shared_mutex>

#include "Client/Client.hpp"
#include "Client/Database/ClientManifest.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Interlink/Interlink.hpp"
#include "Interlink/Database/HealthManifest.hpp"
#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Snapshot/SnapshotService.hpp"
#include "Snapshot/TemporaryMigrationService.hpp"
#include "Transfer/Packet/ClientSwitchPacket.hpp"
#include "Transfer/Packet/ClientTransferPacket.hpp"
#include "Transfer/Packet/EntityTransferPacket.hpp"
#include "Transfer/TransferData.hpp"
#include "Transfer/TransferManifest.hpp"

namespace
{
constexpr auto kEntityTransferRetryInterval = std::chrono::milliseconds(250);

bool CanAcceptIncomingEntityTransfer()
{
	return BoundLeaser::Get().HasBound() &&
		   !TemporaryMigrationService::Get().IsMigrationInProgress();
}
}

void TransferCoordinator::SendEntityPreparePacket(const EntityTransferData& transfer)
{
	EntityTransferPacket etPacket;
	etPacket.TransferID = transfer.ID;
	etPacket.stage = EntityTransferStage::ePrepare;

	auto& PacketData = etPacket.Data.emplace<EntityTransferPacket::PrepareStageData>();
	PacketData.entityIDs = transfer.entityIDs;

	Interlink::Get().SendMessage(transfer.shard, etPacket, NetworkMessageSendFlag::eReliableNow);
}

void TransferCoordinator::SendEntityCommitPacket(const EntityTransferData& transfer)
{
	EntityTransferPacket response;
	response.TransferID = transfer.ID;
	response.stage = EntityTransferStage::eCommit;
	auto& commitData = response.Data.emplace<EntityTransferPacket::CommitStageData>();
	for (const AtlasEntity& entity : transfer.cachedSnapshots)
	{
		commitData.entitySnapshots.push_back({entity, entity.TransferGeneration});
	}

	Interlink::Get().SendMessage(transfer.shard, response, NetworkMessageSendFlag::eReliableNow);
}

void TransferCoordinator::FinalizeSendingEntityTransfer(const EntityTransferData& transfer)
{
	_WriteLock(
		[&]()
		{
			for (const auto& entityID : transfer.entityIDs)
			{
				EntitiesInTransfer.erase(entityID);
			}
			EntityTransfers.erase(transfer.ID);
		});

	if (transfer.sourceBoundID.has_value())
	{
		for (const auto& entityID : transfer.entityIDs)
		{
			SnapshotService::Get().DeleteBoundEntitySnapshot(*transfer.sourceBoundID, entityID);
		}
	}
}

void TransferCoordinator::RollbackSendingEntityTransfer(const EntityTransferData& transfer)
{
	for (const AtlasEntity& entity : transfer.cachedSnapshots)
	{
		if (EntityLedger::Get().ExistsEntity(entity.Entity_ID))
		{
			continue;
		}
		EntityLedger::Get().AddEntity(entity);
	}

	_WriteLock(
		[&]()
		{
			for (const auto& entityID : transfer.entityIDs)
			{
				EntitiesInTransfer.erase(entityID);
			}
			EntityTransfers.erase(transfer.ID);
		});
}

void TransferCoordinator::AbortSendingEntityTransfer(const EntityTransferData& transfer)
{
	_WriteLock(
		[&]()
		{
			for (const auto& entityID : transfer.entityIDs)
			{
				EntitiesInTransfer.erase(entityID);
			}
			EntityTransfers.erase(transfer.ID);
		});
}

bool TransferCoordinator::DoesTargetOwnTransferredEntities(const EntityTransferData& transfer) const
{
	for (const AtlasEntityID& entityID : transfer.entityIDs)
	{
		const std::optional<ShardID> owner = GlobalEntityLedger::Get().GetEntityOwnerShard(entityID);
		if (!owner.has_value() || owner.value() != transfer.shard.ID)
		{
			return false;
		}
	}
	return true;
}

void TransferCoordinator::HandleEntityTransferTimeouts()
{
	std::vector<EntityTransferData> entityTransfers;
	_ReadLock(
		[&]()
		{
			entityTransfers.reserve(EntityTransfers.size());
			for (const auto& [transferID, transfer] : EntityTransfers)
			{
				(void)transferID;
				entityTransfers.push_back(transfer);
			}
		});

	if (entityTransfers.empty())
	{
		return;
	}

	std::vector<std::string> livePeerKeys;
	HealthManifest::Get().GetLivePings(livePeerKeys);
	std::unordered_set<NetworkIdentity> livePeerSet;
	livePeerSet.reserve(livePeerKeys.size());
	for (const std::string& livePeerKey : livePeerKeys)
	{
		ByteReader reader(livePeerKey);
		NetworkIdentity identity;
		identity.Deserialize(reader);
		livePeerSet.insert(std::move(identity));
	}
	const auto now = std::chrono::steady_clock::now();

	for (const EntityTransferData& transfer : entityTransfers)
	{
		if (now - transfer.LastStageAt < kEntityTransferRetryInterval)
		{
			continue;
		}

		const bool peerLive = livePeerSet.contains(transfer.shard);
		if (transfer.transferMode == TransferMode::eReceiving)
		{
			if (!peerLive)
			{
				logger.WarningFormatted(
					"Dropping orphaned receiving entity transfer {} from {} after peer became stale",
					UUIDGen::ToString(transfer.ID), transfer.shard.ToString());
				_WriteLock([&]() { EntityTransfers.erase(transfer.ID); });
			}
			continue;
		}

		switch (transfer.stage)
		{
			case EntityTransferStage::ePrepare:
			{
				if (!peerLive)
				{
					logger.WarningFormatted(
						"Aborting entity transfer {} before commit because target {} is no longer "
						"live",
						UUIDGen::ToString(transfer.ID), transfer.shard.ToString());
					AbortSendingEntityTransfer(transfer);
					break;
				}

				SendEntityPreparePacket(transfer);
				_WriteLock(
					[&]()
					{
						if (auto it = EntityTransfers.find(transfer.ID); it != EntityTransfers.end())
						{
							it->second.LastStageAt = now;
						}
					});
			}
			break;

			case EntityTransferStage::eCommit:
			{
				if (DoesTargetOwnTransferredEntities(transfer))
				{
					logger.WarningFormatted(
						"Finalizing entity transfer {} after target {} already owns the entities",
						UUIDGen::ToString(transfer.ID), transfer.shard.ToString());
					FinalizeSendingEntityTransfer(transfer);
					break;
				}

				if (!peerLive)
				{
					logger.WarningFormatted(
						"Rolling back entity transfer {} because target {} is no longer live",
						UUIDGen::ToString(transfer.ID), transfer.shard.ToString());
					RollbackSendingEntityTransfer(transfer);
					break;
				}

				SendEntityCommitPacket(transfer);
				_WriteLock(
					[&]()
					{
						if (auto it = EntityTransfers.find(transfer.ID); it != EntityTransfers.end())
						{
							it->second.LastStageAt = now;
						}
					});
			}
			break;

			case EntityTransferStage::eNone:
			case EntityTransferStage::eReady:
			case EntityTransferStage::eComplete:
			default:
				break;
		}
	}
}

void TransferCoordinator::ParseEntitiesForTargets()
{
	// Stage 1: Snapshot all entities to process under lock
	std::vector<AtlasEntityID> entitiesToProcess;
	_WriteLock(
		[&]()
		{
			while (!EntitiesToParseForReceiver.empty())
			{
				entitiesToProcess.push_back(EntitiesToParseForReceiver.top());
				EntitiesToParseForReceiver.pop();
			}
		});

	if (entitiesToProcess.empty())
		return;

	// Stage 2: Process entities outside of the transferMutex
	struct EntityProcessData
	{
		AtlasEntityID entityID;
		std::optional<ClientID> clientID;
		std::optional<AtlasTransform> transform;
		std::optional<BoundsID> boundsID;
	};
	std::vector<EntityProcessData> processedEntities;

	for (auto entityID : entitiesToProcess)
	{
		EntityProcessData data;
		data.entityID = entityID;

		// Read entity from EntityLedger outside our lock
		data.transform = EntityLedger::Get()._ReadLock(
			[&]() -> std::optional<AtlasTransform>
			{
				if (EntityLedger::Get().ExistsEntity(entityID))
				{
					auto entityData = EntityLedger::Get().GetEntityMinimal(entityID);
					if (entityData.IsClient)
						data.clientID = entityData.Client_ID;
					return entityData.transform;
				}
				return std::nullopt;
			});

		if (!data.transform.has_value())
			continue;

		// Query heuristic bounds
		data.boundsID = HeuristicManifest::Get().PullHeuristic(
			[&](const IHeuristic& h) { return h.QueryPosition(data.transform->position); });

		if (!data.boundsID.has_value())
			continue;

		processedEntities.push_back(std::move(data));
	}

	if (processedEntities.empty())
		return;

	// Stage 3: Compute ownership map outside lock
	std::unordered_set<BoundsID> allBounds;
	for (auto& e : processedEntities) allBounds.insert(*e.boundsID);

	std::unordered_map<BoundsID, ShardID> ownershipMap;
	if (!allBounds.empty())
	{
		ownershipMap = HeuristicManifest::Get().QueryOwnershipState(
			[&](const HeuristicManifest::OwnershipStateWrapper& w)
			{
				std::unordered_map<BoundsID, ShardID> result;
				for (const BoundsID& bID : allBounds)
				{
					auto shard = w.GetBoundOwner(bID);
					if (shard.has_value() && shard != NetworkCredentials::Get().GetID())
					{
						result[bID] = shard->ID;
					}
				}
				return result;
			});
	}

	// Stage 4: Update TransferCoordinator state under lock
	_WriteLock(
		[&]()
		{
			for (auto& e : processedEntities)
			{
				auto ownershipIt = ownershipMap.find(*e.boundsID);
				if (ownershipIt == ownershipMap.end())
					continue;

				if (e.clientID.has_value())
				{
					ClientTransferData& transfer =
						ClientsPreTransfers.emplace();	// or push to stack
					transfer.ID = UUIDGen::Gen();
					transfer.stage = ClientTransferStage::eNone;
					transfer.transferMode = TransferMode::eSending;
					transfer.shard = NetworkIdentity::MakeIDShard(ownershipIt->second);
					transfer.Clients.push_back({*e.clientID, e.entityID});
					ClientsInTransfer.insert({*e.clientID, transfer.ID});
					EntitiesInTransfer.insert({e.entityID, transfer.ID});
				}
				else
				{
					EntityTransferData& transfer =
						EntityPreTransfers.emplace();  // or push to stack
					transfer.ID = UUIDGen::Gen();
					transfer.stage = EntityTransferStage::eNone;
					transfer.transferMode = TransferMode::eSending;
					transfer.shard = NetworkIdentity::MakeIDShard(ownershipIt->second);
					transfer.entityIDs.push_back(e.entityID);
					if (BoundLeaser::Get().HasBound())
					{
						transfer.sourceBoundID = BoundLeaser::Get().GetBoundID();
					}
					EntitiesInTransfer.insert({e.entityID, transfer.ID});
				}
			}
		});
}
void TransferCoordinator::TransferTick()
{
	// Stage 1: Snapshot all pre-transfers under lock
	std::vector<EntityTransferData> entityTransfersToSend;
	std::vector<ClientTransferData> clientTransfersToSend;

	_WriteLock(
		[&]()
		{
			while (!EntityPreTransfers.empty())
			{
				entityTransfersToSend.push_back(EntityPreTransfers.top());
				EntityPreTransfers.pop();
			}
			while (!ClientsPreTransfers.empty())
			{
				clientTransfersToSend.push_back(ClientsPreTransfers.top());
				ClientsPreTransfers.pop();
			}
		});

	// Stage 2: Process entity transfers outside lock
	for (auto& PreTransfer : entityTransfersToSend)
	{
		SendEntityPreparePacket(PreTransfer);
		PreTransfer.stage = EntityTransferStage::ePrepare;
		PreTransfer.LastStageAt = std::chrono::steady_clock::now();
		TransferManifest::Get().QueueEntityTransferStage(PreTransfer,
														 EntityTransferStage::ePrepare);

		// Stage 3: Insert into internal state under lock
		_WriteLock([&]() { EntityTransfers.insert({PreTransfer.ID, PreTransfer}); });
	}

	// Stage 2: Process client transfers outside lock
	for (auto& PreTransfer : clientTransfersToSend)
	{
		ClientTransferPacket ctPacket;
		ctPacket.TransferID = PreTransfer.ID;
		ctPacket.stage = ClientTransferStage::ePrepare;

		auto& PacketData = ctPacket.Data.emplace<ClientTransferPacket::PrepareStageData>();
		PacketData.clients = PreTransfer.Clients;

		Interlink::Get().SendMessage(PreTransfer.shard, ctPacket,
									 NetworkMessageSendFlag::eReliableNow);

		PreTransfer.stage = ClientTransferStage::ePrepare;

		_WriteLock([&]() { ClientTransfers.insert({PreTransfer.ID, PreTransfer}); });
	}
}
void TransferCoordinator::OnEntityTransferPacketArrival(const EntityTransferPacket& p,
														const PacketManager::PacketInfo& info)
{
	// Stage 1: Snapshot needed internal state under lock
	EntityTransferData transferEntrySnapshot;
	bool hasTransfer = false;

	_WriteLock(
		[&]()
		{
			auto it = EntityTransfers.find(p.TransferID);
			if (it != EntityTransfers.end())
			{
				transferEntrySnapshot = it->second;
				hasTransfer = true;
			}
		});

	// Stage 2: Process outside the lock
	switch (p.stage)
	{
		case EntityTransferStage::eNone:
			ASSERT(false, "This should never happen");
			break;

		case EntityTransferStage::ePrepare:
		{
			if (!CanAcceptIncomingEntityTransfer())
			{
				logger.WarningFormatted(
					"Ignoring entity transfer prepare {} from {} because this shard is not "
					"accepting handoffs right now",
					UUIDGen::ToString(p.TransferID), info.sender.ToString());
				return;
			}

			EntityTransferData data;
			const auto& predataData = p.GetAsPrepareStage();
			data.entityIDs = predataData.entityIDs;
			data.ID = p.TransferID;
			data.shard = info.sender;
			data.transferMode = TransferMode::eReceiving;
			data.stage = EntityTransferStage::eReady;
			data.LastStageAt = std::chrono::steady_clock::now();

			// Send response
			EntityTransferPacket response;
			response.TransferID = p.TransferID;
			response.stage = EntityTransferStage::eReady;
			response.SetAsReadyStage();
			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);
			TransferManifest::Get().QueueEntityTransferStage(data, EntityTransferStage::eReady);

			// Stage 3: Update internal state under lock
			_WriteLock([&]() { EntityTransfers.insert_or_assign(data.ID, data); });
		}
		break;

		case EntityTransferStage::eReady:
		{
			if (!hasTransfer)
			{
				
				logger.ErrorFormatted(
					"Received EntityTransferPacket in Ready Stage for unknown transfer ID with no "
					"internal record: {}",
					UUIDGen::ToString(p.TransferID));
				return;
			}
			if (transferEntrySnapshot.stage == EntityTransferStage::eCommit)
			{
				SendEntityCommitPacket(transferEntrySnapshot);
				_WriteLock(
					[&]()
					{
						auto& entry = EntityTransfers.at(p.TransferID);
						entry.LastStageAt = std::chrono::steady_clock::now();
					});
				return;
			}

			// Collect snapshots from EntityLedger outside lock
			boost::container::small_vector<AtlasEntity, 32> cachedSnapshots;
			const std::optional<BoundsID> sourceBoundID =
				transferEntrySnapshot.sourceBoundID.has_value() ? transferEntrySnapshot.sourceBoundID
																: (BoundLeaser::Get().HasBound()
																	   ? std::optional<BoundsID>(
																			 BoundLeaser::Get().GetBoundID())
																	   : std::nullopt);
			for (const AtlasEntityID EntityID : transferEntrySnapshot.entityIDs)
			{
				if (!EntityLedger::Get().ExistsEntity(EntityID))
				{
					continue;
				}

				AtlasEntity entity = EntityLedger::Get().GetAndEraseEntityForTransfer(EntityID);
				++entity.TransferGeneration;
				if (sourceBoundID.has_value())
				{
					SnapshotService::Get().UpsertBoundEntitySnapshot(*sourceBoundID, entity);
				}
				cachedSnapshots.push_back(entity);
			}

			EntityTransferData stagedTransfer = transferEntrySnapshot;
			stagedTransfer.stage = EntityTransferStage::eCommit;
			stagedTransfer.cachedSnapshots = cachedSnapshots;
			stagedTransfer.sourceBoundID = sourceBoundID;
			stagedTransfer.LastStageAt = std::chrono::steady_clock::now();

			SendEntityCommitPacket(stagedTransfer);
			TransferManifest::Get().QueueEntityTransferStage(transferEntrySnapshot,
															 EntityTransferStage::eCommit);

			// Stage 3: Update internal state
			_WriteLock(
				[&]()
				{
					auto& entry = EntityTransfers.at(p.TransferID);
					entry = std::move(stagedTransfer);
				});
		}
		break;

		case EntityTransferStage::eCommit:
		{
			if (!CanAcceptIncomingEntityTransfer())
			{
				logger.WarningFormatted(
					"Ignoring entity transfer commit {} from {} because this shard is not "
					"accepting handoffs right now",
					UUIDGen::ToString(p.TransferID), info.sender.ToString());
				return;
			}

			EntityTransferData transferEventData;
			if (hasTransfer)
			{
				transferEventData = transferEntrySnapshot;
			}
			else
			{
				transferEventData.ID = p.TransferID;
				transferEventData.shard = info.sender;
				transferEventData.transferMode = TransferMode::eReceiving;
				transferEventData.stage = EntityTransferStage::eCommit;
				for (const auto& data : p.GetAsCommitStage().entitySnapshots)
				{
					transferEventData.entityIDs.push_back(data.Snapshot.Entity_ID);
				}
				logger.WarningFormatted(
					"Received stateless entity commit for transfer {}; applying commit anyway",
					UUIDGen::ToString(p.TransferID));
			}

			// Stage 2: Apply snapshots to EntityLedger outside lock
			const auto& commitData = p.GetAsCommitStage();
			for (const auto& Data : commitData.entitySnapshots)
			{
				AtlasEntity entity = Data.Snapshot;
				entity.TransferGeneration =
					std::max(entity.TransferGeneration, static_cast<uint64_t>(Data.Generation));
				if (EntityLedger::Get().ExistsEntity(entity.Entity_ID))
				{
					continue;
				}
				EntityLedger::Get().AddEntity(entity);
			}

			// Acknowledge commit back to source so it can clear EntitiesInTransfer.
			EntityTransferPacket completeResponse;
			completeResponse.TransferID = p.TransferID;
			completeResponse.stage = EntityTransferStage::eComplete;
			completeResponse.SetAsCompleteStage();
			Interlink::Get().SendMessage(info.sender, completeResponse,
										 NetworkMessageSendFlag::eReliableNow);
			TransferManifest::Get().QueueEntityTransferStage(transferEventData,
															 EntityTransferStage::eComplete);

			// Stage 3: Remove internal record under lock
			_WriteLock([&]() { EntityTransfers.erase(p.TransferID); });
		}
		break;

		case EntityTransferStage::eComplete:
		{
			if (!hasTransfer)
			{
				logger.WarningFormatted(
					"Received EntityTransferPacket in Complete Stage for unknown transfer ID {}",
					UUIDGen::ToString(p.TransferID));
				return;
			}

			if (!DoesTargetOwnTransferredEntities(transferEntrySnapshot))
			{
				logger.WarningFormatted(
					"Deferring entity transfer {} finalization until Entity:EntityOwner shows "
					"target {} as owner",
					UUIDGen::ToString(p.TransferID), transferEntrySnapshot.shard.ToString());
				return;
			}

			FinalizeSendingEntityTransfer(transferEntrySnapshot);
		}
		break;
	}
}
void TransferCoordinator::OnClientTransferPacketArrival(const ClientTransferPacket& p,
														const PacketManager::PacketInfo& info)
{
	// Stage 1: Snapshot internal state
	ClientTransferData transferSnapshot;
	bool hasTransfer = false;

	_WriteLock(
		[&]()
		{
			auto it = ClientTransfers.find(p.TransferID);
			if (it != ClientTransfers.end())
			{
				transferSnapshot = it->second;
				hasTransfer = true;
			}
		});

	// Stage 2: Process outside lock
	switch (p.stage)
	{
		case ClientTransferStage::eNone:
			ASSERT(false, "This should never happen");
			break;

		case ClientTransferStage::ePrepare:
		{
			ClientTransferData newTransfer;
			newTransfer.ID = p.TransferID;
			newTransfer.shard = info.sender;
			newTransfer.stage = ClientTransferStage::eReady;
			newTransfer.transferMode = TransferMode::eReceiving;
			newTransfer.Clients = p.GetAsPrepareStage().clients;

			// Send ready response
			ClientTransferPacket response;
			response.TransferID = p.TransferID;
			response.stage = ClientTransferStage::eReady;
			response.SetAsReadyStage();
			Interlink::Get().SendMessage(info.sender, response,
										 NetworkMessageSendFlag::eReliableNow);

			// Stage 3: Update internal state under lock
			_WriteLock([&]() { ClientTransfers.insert({newTransfer.ID, newTransfer}); });
		}
		break;

		case ClientTransferStage::eReady:
		{
			ASSERT(hasTransfer, "This should never happen");

			// Snapshot for switch
			std::vector<ClientID> clientIDs;
			for (const auto& [clientID, entityID] : transferSnapshot.Clients)
				clientIDs.push_back(clientID);

			// Lookup proxy outside lock
			auto proxyOpt = ClientManifest::Get().GetClientProxy(clientIDs.front());
			ASSERT(proxyOpt.has_value(), "Proxy not found");

			// Send switch packet
			ClientSwitchPacket switchpacket;
			switchpacket.TransferID = p.TransferID;
			switchpacket.stage = ClientSwitchStage::eRequestSwitch;
			auto& switchData = switchpacket.SetAsRequestSwitchStage();
			switchData.clientIDs.assign(clientIDs.begin(), clientIDs.end());
			Interlink::Get().SendMessage(*proxyOpt, switchpacket,
										 NetworkMessageSendFlag::eReliableNow);
		}
		break;

		case ClientTransferStage::eDrained:
		{
			ASSERT(hasTransfer, "This should never happen");

			// Apply entities outside lock
			for (const auto& entity : p.GetAsDrainedStage().clientPayloads)
			{
				EntityLedger::Get().AddEntity(entity);
			}

			// Send activate packet
			auto proxyOpt =
				ClientManifest::Get().GetClientProxy(transferSnapshot.Clients.front().first);
			ASSERT(proxyOpt.has_value(), "Proxy not found");

			ClientSwitchPacket activatePacket;
			activatePacket.TransferID = p.TransferID;
			activatePacket.stage = ClientSwitchStage::eActivate;
			activatePacket.SetAsActivateStage();
			Interlink::Get().SendMessage(*proxyOpt, activatePacket,
										 NetworkMessageSendFlag::eReliableNow);

			// Stage 3: Remove transfer under lock
			_WriteLock([&]() { ClientTransfers.erase(p.TransferID); });
		}
		break;
	}
}
void TransferCoordinator::OnClientSwitchPacketArrival(const ClientSwitchPacket& p,
													  const PacketManager::PacketInfo& info)
{
	// Stage 1: Snapshot internal state
	ClientTransferData transferSnapshot;
	bool hasTransfer = false;

	_WriteLock(
		[&]()
		{
			auto it = ClientTransfers.find(p.TransferID);
			if (it != ClientTransfers.end())
			{
				transferSnapshot = it->second;
				hasTransfer = true;
			}
		});

	// Stage 2: Process outside lock
	switch (p.stage)
	{
		case ClientSwitchStage::eNone:
		case ClientSwitchStage::eRequestSwitch:
		case ClientSwitchStage::eActivate:
			ASSERT(false, "This should never happen");
			break;

		case ClientSwitchStage::eFreeze:
		{
			ASSERT(hasTransfer, "No transfer record");

			std::vector<AtlasEntity> payloads;
			for (const auto& [clientID, entityID] : transferSnapshot.Clients)
			{
				payloads.push_back(EntityLedger::Get().GetAndEraseEntity(entityID));
			}

			ClientTransferPacket response;
			response.TransferID = p.TransferID;
			response.stage = ClientTransferStage::eDrained;
			auto& drainData = response.SetAsDrainedStage();
			drainData.clientPayloads.assign(payloads.begin(), payloads.end());

			Interlink::Get().SendMessage(transferSnapshot.shard, response,
										 NetworkMessageSendFlag::eReliableNow);

			// Stage 3: Update internal state under lock
			_WriteLock(
				[&]()
				{
					for (const auto& [clientID, entityID] : transferSnapshot.Clients)
					{
						ClientsInTransfer.erase(clientID);
						EntitiesInTransfer.erase(entityID);
					}
					ClientTransfers.erase(p.TransferID);
				});
		}
		break;
	}
}
void TransferCoordinator::MarkEntitiesForTransfer(const std::span<AtlasEntityID> entities)
{
	std::unique_lock lock(transferMutex);

	for (const auto& ID : entities) EntitiesToParseForReceiver.push(ID);
}
bool TransferCoordinator::IsEntityInTransfer(AtlasEntityID ID) const
{
	return _ReadLock([&]() { return EntitiesInTransfer.contains(ID); });
}
void TransferCoordinator::TransferThreadEntry(std::stop_token st)
{
	while (!st.stop_requested())
	{
		ParseEntitiesForTargets();
		TransferTick();
		HandleEntityTransferTimeouts();
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

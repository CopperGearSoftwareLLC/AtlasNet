#pragma once

#include "Entity/Entity.hpp"
#include "Entity/EntityHandle.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Network/NetworkIdentity.hpp"
class GlobalEntityLedger : public Singleton<GlobalEntityLedger>
{
	constexpr static const char* entityID2ShardHashMap = "Entity:EntityOwner";
	constexpr static const char* ShardEntitiesList = "Entity:{}_Entities";

   public:
	GlobalEntityLedger() = default;
	void DeclareEntityRecord(const ShardID& NetID, const AtlasEntityID& EntityID)
	{
		// 1. check if entity already exists and has owner
		// 2. update EntityOwner
		// 3. remove from shardEntityList if necesarry
		// 4. add to new shardEntityList

		std::optional<std::string> existingOwner =
			InternalDB::Get()->HGet(entityID2ShardHashMap, UUIDGen::ToString(EntityID));

		InternalDB::Get()->HSet(entityID2ShardHashMap, UUIDGen::ToString(EntityID),
								UUIDGen::ToString(NetID));

		if (existingOwner.has_value())
		{
			InternalDB::Get()->SRem(std::format(ShardEntitiesList, existingOwner.value()),
									{UUIDGen::ToString(EntityID)});
		}
		InternalDB::Get()->SAdd(std::format(ShardEntitiesList, UUIDGen::ToString(NetID)),
								{UUIDGen::ToString(EntityID)});
	}
	void DeleteEntityRecord(const ShardID& NetID, const AtlasEntityID& EntityID)
	{
		// remove from both hash table and shardentitylist only if the NetID matches

		std::optional<std::string> existingOwner =
			InternalDB::Get()->HGet(entityID2ShardHashMap, UUIDGen::ToString(EntityID));

		if (existingOwner.has_value() && existingOwner.value() == UUIDGen::ToString(NetID))
		{
			if (existingOwner.has_value())
			{
				InternalDB::Get()->SRem(std::format(ShardEntitiesList, existingOwner.value()),
										{UUIDGen::ToString(EntityID)});
				InternalDB::Get()->HDel(entityID2ShardHashMap, {UUIDGen::ToString(EntityID)});
			}
		}
	}
	// using back inserter
	void GetAllEntitiesInShard(const ShardID& NetID,
							   std::back_insert_iterator<std::vector<AtlasEntityHandle>> inserter)
	{
		std::vector<std::string> entityIDs =
			InternalDB::Get()->SMembers(std::format(ShardEntitiesList, UUIDGen::ToString(NetID)));
		for (const auto& idStr : entityIDs)
		{
			AtlasEntityHandle handle(UUIDGen::FromString(idStr));
			// how to do emplace using inserter

			inserter = std::move(handle);
		}
	}

	void GetAllEntities(std::back_insert_iterator<std::vector<AtlasEntityHandle>> inserter)
	{
		std::vector<std::string> entityIDStrs = InternalDB::Get()->HKeys(entityID2ShardHashMap);
		for (const auto& idStr : entityIDStrs)
		{
			AtlasEntityHandle handle(UUIDGen::FromString(idStr));
			inserter = std::move(handle);
			/* logger.DebugFormatted("Found EntityID in Global Ledger: {}", idStr); */
		}
	}

	std::optional<ShardID> GetEntityOwnerShard(const AtlasEntityID& EntityID)
	{
		std::optional<std::string> ownerStr =
			InternalDB::Get()->HGet(entityID2ShardHashMap, UUIDGen::ToString(EntityID));
		if (ownerStr.has_value())
		{
			return UUIDGen::FromString(ownerStr.value());
		}
		return std::nullopt;
	}

   private:
};
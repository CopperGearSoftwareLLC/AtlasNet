#pragma once
#include "InternalDB.hpp"
#include "AtlasEntity.hpp"
#include <string>
#include <vector>
#include <optional>
#include <iostream>
/**
 * @brief Manages entity storage and retrieval for individual partitions
 * 
 * This manifest creates a separate hash for each partition to track entities
 * that belong to that partition, providing better organization and visibility.
 */
class PartitionEntityManifest
{
public:
    // Hash key prefix for partition entities
    static constexpr const char* PARTITION_ENTITIES_PREFIX = "partition_entities:";
    
    /**
     * @brief Store entities for a specific partition
     * @param db Database connection
     * @param partitionId Partition identifier (e.g., "ePartition awesome_greider")
     * @param entities Vector of entities to store
     * @return true if successful, false otherwise
     */
    static bool StoreEntities( const std::string& partitionId, 
                             const std::vector<AtlasEntity>& entities)
    {

        
        // Create partition-specific hash key
        std::string partitionKey = PARTITION_ENTITIES_PREFIX + partitionId;
        
        if (entities.empty()) {
            // Clear the hash if no entities to store
            bool cleared = InternalDB::Get()->HDelAll(partitionKey);
            if (!cleared) {
                std::cerr << "[PartitionEntityManifest] WARNING: Failed to clear partition " << partitionId << std::endl;
            }
            return cleared;
        }

        // Convert entities to string format using AtlasEntity::ToString()
        std::string entitiesStr;
        for (const auto& entity : entities) {
            if (!entitiesStr.empty()) entitiesStr.push_back(';');
            entitiesStr += entity.ToString();
        }

        bool result = InternalDB::Get()->HSet(partitionKey, "entities", entitiesStr);
        if (!result) {
            std::cerr << "[PartitionEntityManifest] ERROR: Failed to store entities for partition " << partitionId << std::endl;
        }
        return result;
    }
    
    /**
     * @brief Fetch entities for a specific partition
     * @param db Database connection
     * @param partitionId Partition identifier
     * @return Vector of entities for this partition
     */
    static std::vector<AtlasEntity> FetchEntities( const std::string& partitionId)
    {
        std::vector<AtlasEntity> entities;
        

        // Create partition-specific hash key
        std::string partitionKey = PARTITION_ENTITIES_PREFIX + partitionId;
        
        // Fetch entities string from database
        auto entitiesStr = InternalDB::Get()->HGet(partitionKey, "entities");
        if (!entitiesStr) {
            std::cerr << "[PartitionEntityManifest] WARNING: No entities found for partition " << partitionId << std::endl;
            return entities;
        }

        // Parse entities string
        std::string data = entitiesStr.value();
        if (data.empty()) {
            std::cerr << "[PartitionEntityManifest] WARNING: Empty entities data for partition " << partitionId << std::endl;
            return entities;
        }

        // Parse each entity: "entityId:x,y,z;entityId:x,y,z;..."
        size_t pos = 0;
        while (pos < data.length()) {
            // Find next semicolon or end of string
            size_t semicolon = data.find(';', pos);
            if (semicolon == std::string::npos) semicolon = data.length();
            
            std::string entityStr = data.substr(pos, semicolon - pos);
            if (entityStr.empty()) {
                pos = semicolon + 1;
                continue;
            }
            
            // Parse entity: "entityId:x,y,z"
            size_t colon = entityStr.find(':');
            if (colon == std::string::npos) {
                std::cerr << "[PartitionEntityManifest] ERROR: Invalid entity format: " << entityStr << std::endl;
                pos = semicolon + 1;
                continue;
            }
            
            try {
                AtlasEntity entity = AtlasEntity::FromString(entityStr);
                entities.push_back(entity);
            } catch (const std::exception& e) {
                std::cerr << "[PartitionEntityManifest] ERROR: Failed to parse entity: " << entityStr << " - " << e.what() << std::endl;
            }
            
            pos = semicolon + 1;
        }

        std::cout << "[PartitionEntityManifest] Fetched " << entities.size() 
                  << " entities for partition " << partitionId << std::endl;
        return entities;
    }
    
    /**
     * @brief Add a single entity to a partition
     * @param db Database connection
     * @param partitionId Partition identifier
     * @param entity Entity to add
     * @return true if successful, false otherwise
     */
    static bool AddEntity( const std::string& partitionId, const AtlasEntity& entity)
    {
       

        // Fetch existing entities
        std::vector<AtlasEntity> entities = FetchEntities( partitionId);
        
        // Add new entity
        entities.push_back(entity);
        
        // Store updated list
        return StoreEntities(partitionId, entities);
    }
    
    /**
     * @brief Remove a single entity from a partition
     * @param db Database connection
     * @param partitionId Partition identifier
     * @param entityId ID of entity to remove
     * @return true if successful, false otherwise
     */
    static bool RemoveEntity( const std::string& partitionId, AtlasEntityID entityId)
    {
        

        // Fetch existing entities
        std::vector<AtlasEntity> entities = FetchEntities(partitionId);
    
        // Remove entity with matching ID
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [entityId](const AtlasEntity& e) { return e.ID == entityId; }), entities.end());
        
        // Store updated list
        return StoreEntities(partitionId, entities);
    }
    
    /**
     * @brief Clear all entities for a partition
     * @param db Database connection
     * @param partitionId Partition identifier
     * @return true if successful, false otherwise
     */
    static bool ClearPartition( const std::string& partitionId)
    {

        // Create partition-specific hash key
        std::string partitionKey = PARTITION_ENTITIES_PREFIX + partitionId;
        
        // Check if the key exists first
        bool keyExists = InternalDB::Get()->Exists(partitionKey);
        if (!keyExists) {
            return true; // Success - nothing to clear
        }
        
        // Delete the entire hash
        bool result = InternalDB::Get()->HDelAll(partitionKey);
        if (!result) {
            std::cerr << "[PartitionEntityManifest] ERROR: Failed to clear entities for partition " << partitionId << std::endl;
        }
        return result;
    }
    
    /**
     * @brief Get count of entities for a partition
     * @param db Database connection
     * @param partitionId Partition identifier
     * @return Number of entities in this partition
     */
    static size_t GetEntityCount( const std::string& partitionId)
    {
        std::vector<AtlasEntity> entities = FetchEntities( partitionId);
        return entities.size();
    }
    
    /**
     * @brief Check if a partition has any entities
     * @param db Database connection
     * @param partitionId Partition identifier
     * @return true if partition has entities, false otherwise
     */
    static bool HasEntities( const std::string& partitionId)
    {
        return GetEntityCount( partitionId) > 0;
    }
    
    /**
     * @brief Clear ALL partition entity manifests (nuclear option)
     * @param db Database connection
     * @return true if successful, false otherwise
     */
    static bool ClearAllPartitionManifests()
    {


        //// Find all keys matching the partition entities pattern
        //std::string pattern = std::string(PARTITION_ENTITIES_PREFIX) + "*";
        //std::vector<std::string> keys = db->GetKeysMatching(pattern);
        //
        //bool allCleared = true;
        //for (const auto& key : keys) {
        //    if (!db->HashRemoveAll(key)) {
        //        std::cerr << "[PartitionEntityManifest] ERROR: Failed to clear key: " << key << std::endl;
        //        allCleared = false;
        //    }
        //}
        std::cerr << "PARTITION MANIFEST NOT CLEARED!!!!!!!!" << std::endl;
        return false;
    }
};

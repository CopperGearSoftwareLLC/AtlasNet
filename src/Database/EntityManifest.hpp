#pragma once
#include "pch.hpp"
#include "IDatabase.hpp"
#include "AtlasNet/AtlasEntity.hpp"

/**
 * @brief Database manifest for entity outliers
 */
class EntityManifest
{
public:
    static inline const std::string OUTLIERS_HASH = "entity_outliers";
    static inline const std::string ENTITIES_SNAPSHOT_HASH = "entities_snapshot";

    /**
     * @brief Remove a specific entity from a partition's outliers
     */
    static bool RemoveEntityFromOutliers(IDatabase *db, const std::string &partitionId, AtlasEntityID entityId)
    {
        if (!db)
            return false;
        std::string data = db->HashGet(OUTLIERS_HASH, partitionId);
        if (data.empty())
            return true;
        std::vector<std::string> entries;
        size_t start = 0;
        bool removed = false;
        while (start < data.size())
        {
            size_t end = data.find(';', start);
            std::string entry = data.substr(start, end - start);
            size_t colon = entry.find(':');
            if (colon != std::string::npos)
            {
                AtlasEntityID id = static_cast<AtlasEntityID>(std::stoul(entry.substr(0, colon)));
                if (id == entityId)
                {
                    removed = true;
                }
                else
                {
                    entries.push_back(entry);
                }
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        // Rebuild string
        std::string newData;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i > 0)
                newData.push_back(';');
            newData += entries[i];
        }
        return db->HashSet(OUTLIERS_HASH, partitionId, newData);
    }

    /**
     * @brief Store a single entity for a partition (appends to outliers hash)
     */
    static bool StoreEntity(IDatabase *db, const std::string &partitionId, const AtlasEntity &entity)
    {
        if (!db)
            return false;
        // Fetch current outliers string
        std::string current = db->HashGet(OUTLIERS_HASH, partitionId);
        if (!current.empty())
            current.push_back(';');
        current += std::to_string(entity.ID) + ":" +
                   std::to_string(entity.Position.x) + "," +
                   std::to_string(entity.Position.z);
        bool result = db->HashSet(OUTLIERS_HASH, partitionId, current);
        if (result)
        {
            std::cout << "[EntityManifest] Stored entity " << entity.ID << " in partition " << partitionId << std::endl;
        }
        return result;
    }

    /**
     * @brief Store entity outliers for a partition
     */
    static bool StoreOutliers(IDatabase *db, const std::string &partitionId,
                              const std::vector<AtlasEntity> &outliers)
    {
        if (!db)
        {
            std::cerr << "[EntityManifest] ERROR: Database is null" << std::endl;
            return false;
        }
        if (outliers.empty())
        {
            std::cerr << "[EntityManifest] WARNING: No outliers to store" << std::endl;
            return true;
        }

        std::string outliersStr;
        for (const auto &entity : outliers)
        {
            if (!outliersStr.empty())
                outliersStr.push_back(';');
            outliersStr += std::to_string(entity.ID) + ":" +
                           std::to_string(entity.Position.x) + "," +
                           std::to_string(entity.Position.z);
        }

        std::cerr << "[EntityManifest] DEBUG: Attempting to store outliers for partition " << partitionId
                  << " with " << outliers.size() << " entities" << std::endl;
        std::cerr << "[EntityManifest] DEBUG: Outliers string: " << outliersStr << std::endl;

        bool result = db->HashSet(OUTLIERS_HASH, partitionId, outliersStr);
        if (result)
        {
            std::cout << "[EntityManifest] Stored " << outliers.size() << " entities in partition " << partitionId << std::endl;
        }
        else
        {
            std::cerr << "[EntityManifest] ERROR: Failed to store outliers in database" << std::endl;
        }
        return result;
    }

    /**
     * @brief Fetch outliers for a partition
     */

    // Returns vector of AtlasEntity
    static std::vector<AtlasEntity> FetchOutliers(IDatabase *db, const std::string &partitionId)
    {
        std::vector<AtlasEntity> result;
        if (!db)
            return result;
        std::string data = db->HashGet(OUTLIERS_HASH, partitionId);
        if (data.empty())
            return result;
        size_t start = 0;
        while (start < data.size())
        {
            size_t end = data.find(';', start);
            std::string entry = data.substr(start, end - start);
            size_t colon = entry.find(':');
            size_t comma = entry.find(',');
            if (colon != std::string::npos && comma != std::string::npos && colon < comma)
            {
                AtlasEntity e;
                e.ID = static_cast<AtlasEntityID>(std::stoul(entry.substr(0, colon)));
                e.Position.x = std::stof(entry.substr(colon + 1, comma - (colon + 1)));
                e.Position.y = 0.0f;
                e.Position.z = std::stof(entry.substr(comma + 1));
                result.push_back(e);
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
        return result;
    }

    /**
     * @brief Clear all outliers from a partition
     */
    static bool ClearOutliers(IDatabase *db, const std::string &partitionId)
    {
        if (!db)
            return false;
        return db->HashRemove(OUTLIERS_HASH, partitionId);
    }

    /**
     * @brief Clear all outliers from all partitions
     */
    static bool ClearAllOutliers(IDatabase *db)
    {
        if (!db)
            return false;
        return db->HashRemoveAll(OUTLIERS_HASH);
    }

    /**
     * @brief Store a read-only snapshot of all entities for a partition
     */
    static bool StoreEntitiesSnapshot(
        IDatabase *db,
        const std::string &partitionId,
        const std::vector<AtlasEntity> &entities)
    {
        if (!db)
            return false;

        nlohmann::json j = nlohmann::json::array();

        for (const auto &e : entities)
            j.push_back(e.ToJson()); // uses to_json(AtlasEntity)

        return db->HashSet(ENTITIES_SNAPSHOT_HASH, partitionId, j.dump());
    }
    static bool RemoveEntitiesSnapshot(IDatabase *db, const std::string &partitionId)
    {
        if (!db)
            return false;
        // Build compact string id:x,z;id:x,z
        std::string data;

        return db->HashRemove(ENTITIES_SNAPSHOT_HASH, partitionId);
    }
    static bool GetAllEntitiesSnapshot(IDatabase *db,
                                       std::unordered_map<std::string, std::vector<AtlasEntity>> &entities)
    {
        if (!db)
            return false;

        entities.clear();
        const auto allData = db->HashGetAll(ENTITIES_SNAPSHOT_HASH);

        for (const auto &[partitionId, jsonString] : allData)
        {
            try
            {
                nlohmann::json j = nlohmann::json::parse(jsonString);

                if (!j.is_array())
                    continue;

                std::vector<AtlasEntity> list;
                list.reserve(j.size());

                for (const auto &elem : j)
                {
                    AtlasEntity e = AtlasEntity::FromJson(elem);
                    list.push_back(std::move(e));
                }

                entities[partitionId] = std::move(list);
            }
            catch (...)
            {
                // malformed JSON: skip this entry
                continue;
            }
        }

        return true;
    }
};
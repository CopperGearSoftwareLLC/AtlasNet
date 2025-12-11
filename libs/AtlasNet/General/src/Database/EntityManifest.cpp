#include "EntityManifest.hpp"
#include "Debug/Log.hpp"
#include "Database/PartitionEntityManifest.hpp"
#include "Database/ServerRegistry.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "Database/RedisCacheDatabase.hpp"

bool EntityManifest::PushManagedEntitiesSnapshot(
    IDatabase* db,
    const std::string& partitionId,
    const std::vector<AtlasEntity>& managedEntities,
    std::chrono::steady_clock::time_point& lastPushTime,
    std::shared_ptr<Log> logger,
    int snapshotIntervalSeconds)
{
    // Push every N seconds if we have entities
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPushTime).count() < snapshotIntervalSeconds)
    {
        return false; // Skipped - too soon
    }

    if (managedEntities.empty())
    {
        lastPushTime = now;
        return false; // Skipped - no entities
    }

    if (!db)
    {
        if (logger) logger->Error("Database connection is null - cannot push entities snapshot");
        return false;
    }

    // Ensure database is alive
    if (!db->Exists("connection_test")) {
        if (logger) {
            logger->Error("Database connection test failed during entities snapshot - reconnecting");
        }
        return false;
    }

    std::string partitionKey = partitionId + ":entities_snapshot";
    if (StoreEntitiesSnapshot(db, partitionKey, managedEntities))
    {
        if (logger) {
            logger->DebugFormatted("SNAPSHOT: Pushed {} managed entities to read-only snapshot for {}", 
                managedEntities.size(), partitionKey);
        }
        lastPushTime = now;
        return true;
    }
    else
    {
        if (logger) {
            logger->ErrorFormatted("SNAPSHOT: Failed to push managed entities snapshot for {}", partitionKey);
        }
        return false;
    }
}

std::vector<AtlasEntity> EntityManifest::FetchAllEntitiesFromAllPartitions(
    IDatabase* db,
    bool includeOutliers,
    bool includeSnapshots)
{
    std::vector<AtlasEntity> allEntities;

    if (!db) {
        return allEntities;
    }

    // Get all partition servers from registry
    const auto& servers = ServerRegistry::Get().GetServers();
    std::vector<std::string> partitionIds;

    // Filter to get only partition IDs
    for (const auto& server : servers) {
        if (server.first.Type == InterlinkType::ePartition) {
            partitionIds.push_back(server.first.ToString());
        }
    }

    // Fetch entities from each partition
    for (const auto& partitionId : partitionIds) {
        // Fetch managed entities from PartitionEntityManifest
        std::vector<AtlasEntity> managedEntities = PartitionEntityManifest::FetchEntities(db, partitionId);
        allEntities.insert(allEntities.end(), managedEntities.begin(), managedEntities.end());

        // Fetch outliers if requested
        if (includeOutliers) {
            std::vector<AtlasEntity> outliers = FetchOutliers(db, partitionId);
            allEntities.insert(allEntities.end(), outliers.begin(), outliers.end());
        }

        // Fetch snapshots if requested
        if (includeSnapshots) {
            std::string snapshotKey = partitionId + ":entities_snapshot";
            std::optional<std::string> snapshotData = db->HashGet(ENTITIES_SNAPSHOT_HASH, snapshotKey);
            if (snapshotData.has_value() && !snapshotData->empty()) {
                // Parse snapshot data: "id:x,z;id:x,z;..."
                std::string data = snapshotData.value();
                size_t start = 0;
                while (start < data.size()) {
                    size_t end = data.find(';', start);
                    std::string entry = data.substr(start, (end == std::string::npos ? data.length() : end) - start);
                    size_t colon = entry.find(':');
                    size_t comma = entry.find(',');
                    if (colon != std::string::npos && comma != std::string::npos && colon < comma) {
                        try {
                            AtlasEntity e;
                            e.ID = static_cast<AtlasEntityID>(std::stoul(entry.substr(0, colon)));
                            e.Position.x = std::stof(entry.substr(colon + 1, comma - (colon + 1)));
                            e.Position.y = 0.0f;
                            e.Position.z = std::stof(entry.substr(comma + 1));
                            allEntities.push_back(e);
                        } catch (const std::exception&) {
                            // Skip invalid entries
                        }
                    }
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
            }
        }
    }

    return allEntities;
}

std::vector<AtlasEntity> EntityManifest::DeduplicateEntities(const std::vector<AtlasEntity>& entities)
{
    if (entities.empty()) {
        return entities;
    }

    std::unordered_map<AtlasEntityID, AtlasEntity> uniqueEntities;
    for (const auto& entity : entities) {
        // Keep the last occurrence of each entity ID
        uniqueEntities[entity.ID] = entity;
    }

    std::vector<AtlasEntity> result;
    result.reserve(uniqueEntities.size());
    for (const auto& [id, entity] : uniqueEntities) {
        result.push_back(entity);
    }

    return result;
}


#pragma once

#include <memory>
#include <atomic>
#include <set>
#include <chrono>
#include <unordered_map>

#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Heuristic/Shape.hpp"
#include "Heuristic/GridCell.hpp"
#include "Interlink/Connection.hpp"
#include "Interlink/InterlinkEnums.hpp"
#include "AtlasNet/AtlasNet.hpp"
#include "AtlasNet/AtlasEntity.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"

class Partition : public Singleton<Partition>
{
    std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
    std::atomic_bool ShouldShutdown = false;
    std::unique_ptr<InterLinkIdentifier> ConnectedGameServer;
    std::unordered_set<InterLinkIdentifier> ConnectedProxies;
    
    std::unique_ptr<IDatabase> database;
    std::chrono::steady_clock::time_point lastEntitiesSnapshotPush;

public:
    Shape partitionShape;
    GridShape partitionGridShape;

    // Primary State: Map for O(1) access
    std::unordered_map<AtlasEntityID, AtlasEntity> managedEntities;

    // Outlier Tracking
    std::set<AtlasEntityID> reportedOutliers;
    std::chrono::steady_clock::time_point lastOutlierNotification;

    Partition();
    ~Partition();
    void Init();
    void Shutdown() { ShouldShutdown = true; }
    void MessageArrived(const Connection &fromWhom, std::span<const std::byte> data);

private:
    // --- Core Logic Helpers (Consolidated) ---
    
    // Adds to Map + Updates Database + Clears Outlier status
    void RegisterEntity(const AtlasEntity& entity);
    
    // Removes from Map + Removes from Database + Clears Outlier status
    void UnregisterEntity(AtlasEntityID id);

    // Network Helpers
    void NotifyProxiesIncoming(const AtlasEntity& entity, const InterLinkIdentifier* specificTarget = nullptr);
    void NotifyProxiesOutgoing(const std::vector<AtlasEntity>& entities);
    void NotifyGodOfOutliers(const std::vector<AtlasEntity>& outliers);

    // --- Existing Helpers ---
    bool ParseEntityPacket(std::span<const std::byte> data,
                           AtlasNetMessageHeader &outHeader,
                           std::vector<AtlasEntity> &outEntities);
    
    void checkForOutliersAndNotifyGod();
    void notifyGodAboutOutliers();
    void pushManagedEntitiesSnapshot();
    bool IsEntityInside(const AtlasEntity& entity);
    InterLinkIdentifier getCurrentPartitionId() const;
};
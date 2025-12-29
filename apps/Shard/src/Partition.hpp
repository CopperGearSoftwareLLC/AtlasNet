#pragma once

#include <memory>
#include <atomic>
#include <set>
#include <chrono>

#include "Misc/Singleton.hpp"
#include "Log.hpp"
#include "Shape.hpp"
#include "GridCell.hpp"
#include "Connection.hpp"
#include "InterlinkEnums.hpp"
#include "AtlasNet.hpp"
#include "AtlasEntity.hpp"
#include <unordered_map>
#include <unordered_set>

class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
	std::atomic_bool ShouldShutdown = false;
  std::unique_ptr<InterLinkIdentifier> ConnectedGameServer;
  std::unordered_set<InterLinkIdentifier> ConnectedProxies;
    // Periodic snapshot timer for read-only entity push
    std::chrono::steady_clock::time_point lastEntitiesSnapshotPush;
  public:
	Shape partitionShape;
	GridShape partitionGridShape;  // New grid cell-based shape
	// Entities managed by this partition (Atlas entities spawned inside an expanded bounding box)
	std::vector<AtlasEntity> managedEntities;
	// Track which entities have already been reported as outliers
	std::set<AtlasEntityID> reportedOutliers;
	// Track when we last notified God about outliers
	std::chrono::steady_clock::time_point lastOutlierNotification;
	Partition();
	~Partition();
	void Init();
	void Shutdown() {ShouldShutdown = true;}
	void MessageArrived(const Connection &fromWhom, std::span<const std::byte> data);

private:
  bool ParseEntityPacket(std::span<const std::byte> data,
                          AtlasNetMessageHeader &outHeader,
                          std::vector<AtlasEntity> &outEntities);
	void checkForOutliersAndNotifyGod();
	void notifyGodAboutOutliers();
	void pushManagedEntitiesSnapshot();
	
	// Helper method to get current partition identifier
	InterLinkIdentifier getCurrentPartitionId() const;
};
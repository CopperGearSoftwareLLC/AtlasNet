#pragma once

#include <memory>
#include <atomic>
#include <set>
#include <chrono>

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
#undef ATLAS_UNITY_PLUGIN
#if defined(ATLAS_UNITY_PLUGIN)
// Plugin build — use absolute path for now
#include "/mnt/d/KDNet/KDNet/src/AtlasNet/AtlasEntity.hpp"
#else
// Normal AtlasNet build — use relative include
#include "AtlasNet/AtlasEntity.hpp"
#endif

class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
	std::atomic_bool ShouldShutdown = false;
  std::vector<AtlasEntity> CachedEntities;
  std::unique_ptr<InterLinkIdentifier> ConnectedGameServer;
	// Persistent database connection to avoid connection issues
	std::unique_ptr<IDatabase> database;
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
	
	// Helper method to get current partition identifier
	InterLinkIdentifier getCurrentPartitionId() const;
};
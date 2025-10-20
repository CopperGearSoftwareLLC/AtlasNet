#pragma once

#include <memory>
#include <atomic>

#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Heuristic/Shape.hpp"
#include "Interlink/Connection.hpp"
#include "Interlink/InterlinkEnums.hpp"

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
  public:
	Shape partitionShape;
	Partition();
	~Partition();
	void Init();
	void Shutdown() {ShouldShutdown = true;}
	void MessageArrived(const Connection &fromWhom, std::span<const std::byte> data);
};
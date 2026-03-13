#pragma once

#include <stop_token>
#include <thread>

#include "Debug/Log.hpp"
#include "Global/Misc/Singleton.hpp"

class HotspotSnapshotService : public Singleton<HotspotSnapshotService>
{
	Log logger = Log("HotspotSnapshotService");
	std::jthread computeThread;

	// Legacy class name, but this service now emits generic recompute snapshot
	// records for hotspot generation and stores them asynchronously.
	void ComputeThreadLoop(std::stop_token st);
	void ComputeAndStoreSnapshot();

   public:
	HotspotSnapshotService();
};

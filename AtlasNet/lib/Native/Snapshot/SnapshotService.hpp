#pragma once

#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IBounds.hpp"
class SnapshotService : public Singleton<SnapshotService>
{
	static constexpr const char* EntitySnapshotBoundsIndex_HashTable =
		"Entity:Snapshot:Bounds";
	Log logger = Log("SnapshotService");
	std::jthread snapshotThread;
	std::mutex recoveryMutex;
	std::optional<BoundsID> recoveredBoundID;

   public:
	SnapshotService();
	void RecoverClaimedBoundSnapshotIfNeeded();
	void UpsertBoundEntitySnapshot(BoundsID boundID, const AtlasEntity& entity);
	void UpsertClaimedBoundEntitySnapshot(const AtlasEntity& entity);
	void DeleteBoundEntitySnapshot(BoundsID boundID, const AtlasEntityID& entityID);
	void DeleteClaimedBoundEntitySnapshot(const AtlasEntityID& entityID);

	void FetchEntityListSnapshot(
		std::unordered_map<BoundsID, std::vector<AtlasEntity>>& data);

	void FetchBoundsTransformList(
		std::unordered_map<BoundsID, std::vector<AtlasTransform>>& transforms);

	void FetchAllTransforms(std::vector<AtlasTransform>& transforms);

   private:
	void SnapshotThreadLoop(std::stop_token st);
	bool RecoverBoundSnapshot(BoundsID boundID);
	void ReconcileClaimedBoundEntityRecords();
	void UploadSnapshot();
	void TouchBoundSnapshotIndex(BoundsID boundID);
	void RemoveBoundSnapshotIndex(BoundsID boundID);
};

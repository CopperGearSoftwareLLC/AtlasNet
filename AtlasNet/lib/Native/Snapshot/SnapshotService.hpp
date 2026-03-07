#pragma once

#include <stop_token>
#include <thread>
#include <unordered_map>

#include "Entity/Entity.hpp"
#include "Entity/Transform.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Heuristic/IBounds.hpp"
class SnapshotService : public Singleton<SnapshotService>
{
	const char* const SnapshotBoundsID_2_EntityList_Entry_HashTable =
		"Snapshot:BoundIDs -> EntityList";
	const char* const SnapshotBoundsID_2_Transform_Entry_HashTable =
		"Snapshot:BoundIDs -> Transforms";
	std::jthread snapshotThread;

   public:
	SnapshotService();

	void FetchEntityListSnapshot(
		std::unordered_map<BoundsID, std::vector<AtlasEntity>>& data);

	void FetchBoundsTransformList(
		std::unordered_map<BoundsID, std::vector<Transform>>& transforms);

	void FetchAllTransforms(std::vector<Transform>& transforms);

   private:
	void SnapshotThreadLoop(std::stop_token st);

	void UploadSnapshot();
};
#include "SnapshotService.hpp"

#include <boost/container/static_vector.hpp>
#include <charconv>
#include <chrono>
#include <cstring>
#include <execution>
#include <string>
#include <thread>

#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Transform.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Heuristic/BoundLeaser.hpp"
#include "Heuristic/IBounds.hpp"
#include "InternalDB/InternalDB.hpp"
void SnapshotService::SnapshotThreadLoop(std::stop_token st)
{
	using namespace std::chrono;
	const milliseconds interval(_ATLASNET_SNAPSHOT_INTERNAL_MS);
	while (!st.stop_requested())
	{
		// Align roughly to system clock
		auto now = steady_clock::now();
		auto now_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

		// Compute time until next "tick" of interval
		auto next_tick_ms = ((now_ms.count() / interval.count()) + 1) * interval.count();
		auto sleep_duration = milliseconds(next_tick_ms - now_ms.count());

		// Add tiny random jitter to avoid perfect collisions
		sleep_duration += milliseconds(rand() % 10);  // 0-9 ms jitter

		// Sleep until next tick
		if (sleep_duration > milliseconds(0))
			std::this_thread::sleep_for(sleep_duration);

		if (st.stop_requested())
			break;

		UploadSnapshot();
	}
}
SnapshotService::SnapshotService()
{
	snapshotThread = std::jthread([this](std::stop_token st) { SnapshotThreadLoop(st); });
};

void SnapshotService::UploadSnapshot()
{
	if (BoundLeaser::Get().HasBound())
	{
		BoundsID boundID = BoundLeaser::Get().GetBoundID();
		ByteWriter entityListWriter;
		ByteWriter transformWriter;
		boost::container::static_vector<char, 32> boundString;
		boundString.resize(32);
		if (EntityLedger::Get().GetEntityCount() > 0)
		{
			EntityLedger::Get().ForEachEntityRead(std::execution::seq,
												  [&](const AtlasEntity& entity)
												  {
													  entity.Serialize(entityListWriter);
													  entity.transform.Serialize(transformWriter);
												  });

			std::to_chars(boundString.data(), boundString.data() + boundString.size(), boundID);
			InternalDB::Get()->HSet(SnapshotBoundsID_2_EntityList_Entry_HashTable,
									boundString.data(), entityListWriter.as_string_view());
			InternalDB::Get()->HSet(SnapshotBoundsID_2_Transform_Entry_HashTable,
									boundString.data(), transformWriter.as_string_view());
		}
		else
		{
			InternalDB::Get()->HDel(SnapshotBoundsID_2_EntityList_Entry_HashTable,
									{boundString.data()});
			InternalDB::Get()->HDel(SnapshotBoundsID_2_Transform_Entry_HashTable,
									{boundString.data()});
		}
	}
	else
	{
	}
}
void SnapshotService::FetchEntityListSnapshot(
	std::unordered_map<BoundsID, std::vector<AtlasEntity>>& data)
{
	data.clear();
	const std::unordered_map<std::string, std::string> keyvals =
		InternalDB::Get()->HGetAll(SnapshotBoundsID_2_EntityList_Entry_HashTable);

	data.reserve(keyvals.size());
	for (const auto& [Key, Val] : keyvals)
	{
		const BoundsID ID = std::stoi(Key);
		data.emplace(ID, std::vector<AtlasEntity>{});
		auto& vec = data.at(ID);
		ByteReader br(Val);
		while (br.remaining())
		{
			vec.emplace_back(AtlasEntity{}).Deserialize(br);
		}
	}
}
void SnapshotService::FetchBoundsTransformList(
	std::unordered_map<BoundsID, std::vector<Transform>>& transforms)
{
	transforms.clear();
	const std::unordered_map<std::string, std::string> keyvals =
		InternalDB::Get()->HGetAll(SnapshotBoundsID_2_Transform_Entry_HashTable);

	transforms.reserve(keyvals.size());
	for (const auto& [Key, Val] : keyvals)
	{
		const BoundsID ID = std::stoi(Key);
		transforms.emplace(ID, std::vector<Transform>{});
		auto& vec = transforms.at(ID);
		ByteReader br(Val);
		while (br.remaining())
		{
			vec.emplace_back(Transform{}).Deserialize(br);
		}
	}
}
void SnapshotService::FetchAllTransforms(std::vector<Transform>& transforms)
{
	transforms.clear();
	const std::unordered_map<std::string, std::string> keyvals =
		InternalDB::Get()->HGetAll(SnapshotBoundsID_2_Transform_Entry_HashTable);

	for (const auto& [Key, Val] : keyvals)
	{
		ByteReader br(Val);
		transforms.emplace_back(Transform{}).Deserialize(br);
	}
}

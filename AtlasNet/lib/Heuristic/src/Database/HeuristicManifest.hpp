#pragma once

#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Heuristic/IBounds.hpp"
#include "Heuristic/IHeuristic.hpp"
#include "InternalDB.hpp"
#include "Misc/Singleton.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"

class HeuristicManifest : public Singleton<HeuristicManifest>
{
	/*
	All available bounds are placed in pending.
	At first or on change. each Shard goes and Pops from the pending list. Adding it to Claimed with
	their key.
	*/

	const std::string HeuristicTypeKey = "Heuristic_Type";
	/*stores a list of available, unclaimed bounds*/
	/*the bounds id is the shape*/
	const std::string PendingHashTable = "Heuristic_Bounds_Pending";

	/*HashTable of claimed Bounds*/
	const std::string ClaimedHashTable = "Heuristic_Bounds_Claimed";

   public:
	[[nodiscard]] IHeuristic::Type GetActiveHeuristicType() const;
	void SetActiveHeuristicType(IHeuristic::Type type);

	template <typename BoundType, typename KeyType = std::string>
	void GetAllPendingBounds(std::vector<BoundType>& out_bounds);
	template <typename BoundType, typename KeyType = std::string>
	void StorePendingBounds(const std::vector<BoundType>& in_bounds);
	void StorePendingBoundsFromByteWriters(
		const std::unordered_map<IBounds::BoundsID, ByteWriter>& in_writers);
	void GetPendingBoundsAsByteReaders(std::vector<std::string>& data_for_readers,
									   std::unordered_map<IBounds::BoundsID, ByteReader>& brs);

	template <typename BoundType, typename KeyType = std::string>
	void GetAllClaimedBounds(std::unordered_map<KeyType, BoundType>& out_bounds);

	// IHeuristic::Type GetActiveHeuristic() const;
	// template <typename KeyType = std::string>
	// void WriteBounds(const KeyType& key, const IBounds& bound)
	//{
	//	ByteWriter bw;
	//	bound.Serialize(bw);
	//	InternalDB::Get()->HSet(BoundsTableKey, key, bw.bytes());
	// }
	//
	// template <typename BoundType, typename KeyType = std::string>
	// void WriteNBounds(const std::unordered_map<KeyType, BoundType>& bounds)
	//{
	//	for (const auto& [key, bound] : bounds)
	//	{
	//		WriteBounds(key, bound);
	//	}
	//}
	//
	// template <typename BoundType, typename KeyType = std::string>
	// void GetBounds(const KeyType& key, const BoundType& bounds)
	//{
	//	const bool result = TryGetBounds(key, bounds);
	//	ASSERT(result, "Failed to get bounds");
	//}
	// template <typename BoundType, typename KeyType = std::string>
	// bool TryGetBounds(const KeyType& key, const BoundType& bounds)
	//{
	//	try
	//	{
	//		const auto getResult = InternalDB::Get()->HGet(BoundsTableKey, key);
	//		ByteReader br(getResult.value());
	//		bounds.Deserialize(br);
	//	}
	//	catch (...)
	//	{
	//		return false;
	//	}
	//}
	// template <typename BoundType, typename KeyType = std::string>
	// void GetAllBounds(std::unordered_map<KeyType, BoundType>& out_bounds)
	//{
	//	out_bounds.clear();
	//	const auto BoundsTable = InternalDB::Get()->HGetAll(BoundsTableKey);
	//	out_bounds.reserve(BoundsTable.size());
	//
	//	for (const auto& [key, boundsString] : BoundsTable)
	//	{
	//		const auto in = out_bounds.emplace(key, BoundType());
	//		const BoundType& b = out_bounds.at(in);
	//		ByteReader br(boundsString);
	//		b.Deserialize(br);
	//	}
	//}
	// template <typename BoundType, typename KeyType = std::string>
	// bool TryGetAllBounds(std::unordered_map<KeyType, BoundType>& out_bounds)
	//{
	//	try
	//	{
	//		out_bounds.clear();
	//		const auto BoundsTable = InternalDB::Get()->HGetAll(BoundsTableKey);
	//		out_bounds.reserve(BoundsTable.size());
	//
	//		for (const auto& [key, boundsString] : BoundsTable)
	//		{
	//			const auto in = out_bounds.emplace(key, BoundType());
	//			const BoundType& b = out_bounds.at(in);
	//			ByteReader br(boundsString);
	//			b.Deserialize(br);
	//		}
	//	}
	//	catch (...)
	//	{
	//		return false;
	//	}
	//	return true;
	//};
};
template <typename BoundType, typename KeyType>
inline void HeuristicManifest::GetAllPendingBounds(std::vector<BoundType>& out_bounds)
{
	out_bounds.clear();
	std::vector<std::string> data_for_readers;
	std::unordered_map<IBounds::BoundsID, ByteReader> brs;
	GetPendingBoundsAsByteReaders(data_for_readers, brs);
	const auto PendingBoundsSet = InternalDB::Get()->HGetAll(PendingHashTable);
	out_bounds.reserve(PendingBoundsSet.size());
	for (auto& [bound_id, boundByteReader] : brs)
	{
		out_bounds.emplace_back(BoundType());
		BoundType& b = out_bounds.back();
		b.Deserialize(boundByteReader);
	}
}
template <typename BoundType, typename KeyType>
void HeuristicManifest::StorePendingBounds(const std::vector<BoundType>& in_bounds)
{
	std::string_view s_b, s_id;
	for (int i = 0; i < in_bounds.size(); i++)
	{
		const auto& bounds = in_bounds[i];

		ByteWriter bw;
		bounds.Serialize(bw);
		s_b = bw.as_string_view();

		ByteWriter bw_id;
		bw_id.u32(bounds.GetID());
		s_id = bw_id.as_string_view();
		InternalDB::Get()->HSet(PendingHashTable, s_id, s_b);
	}
}

template <typename BoundType, typename KeyType>
void HeuristicManifest::GetAllClaimedBounds(std::unordered_map<KeyType, BoundType>& out_bounds)
{
	out_bounds.clear();
	const auto BoundsTable = InternalDB::Get()->HGetAll(ClaimedHashTable);
	out_bounds.reserve(BoundsTable.size());

	for (const auto& [key, boundsString] : BoundsTable)
	{
		const auto in = out_bounds.emplace(key, BoundType());
		const BoundType& b = out_bounds.at(in);
		ByteReader br(boundsString);
		b.Deserialize(br);
	}
}